// Quyen19492 Item Sorting -- D2RLoader plugin entry point.
//
// Wires the pure algorithm in sort_engine.hpp/.cpp to the official
// D2RLoader Plugin SDK: no native_hook / InstallInlineHook / mutations.h
// usage anywhere in this plugin (see README "Why no native hooks").
//
// Threading: item mutation (ItemService::executeExistingItemTransaction)
// must run on the authoritative game thread (ThreadService::runOnGameThread);
// widget calls (WidgetService, PanelService::openPanel) must run on the UI
// thread (ThreadService::runOnUiThread). The three button actions therefore
// hop game-thread -> (compute + apply) -> UI-thread -> (show toast). The two
// "selected tab" arrow buttons need no item mutation, so their handlers run
// directly from the UI-thread UiMessageListener callback that receives them.
//
// See README "Known gaps" for the v1 scope notes this file's comments
// reference (item-classification approximations, no live-tested widget
// positions, multiplayer-client relay not implemented yet, etc).

#include <D2RLPlugin/api.h>

#include "grid_config_toml.hpp"
#include "grid_sort.hpp"
#include "item_classification.hpp"
#include "sort_engine.hpp"
#include "ui_layouts.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace D2RL;

namespace {

constexpr PluginInfo kPluginInfo {
	.infoSize    = PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = ItemSorting::UiLayouts::kPluginId,
	.name        = "Item Sorting",
	.version     = "0.1.0",
	.author      = "quyen19492",
	.description = "Auto-sorts the shared stash, personal stash, inventory and Horadric Cube.",
	.flags       = PluginFlags::Shared,
};

const InventoryService*   g_inventory    = nullptr;
const ItemService*        g_items        = nullptr;
const ThreadService*      g_threads      = nullptr;
const PanelService*       g_panels       = nullptr;
const WidgetService*      g_widgets      = nullptr;
const SharedEventService* g_sharedEvents = nullptr;

Panels::RegistrationHandle  g_toastPanel         = Panels::InvalidHandle;
SharedEvents::ListenerHandle g_uiMessageListener = SharedEvents::InvalidHandle;

// Invisible (1x1) "trigger" panels, one per button. See the "Known
// gaps" #6 iteration 7 note in plugin.cpp's OnUiMessage: a button's
// onClickMessage can only target a small set of D2R-native "message
// channels" (PanelManager is one); a plugin-invented target string is
// rejected by the engine's own TypeDesc validation before it is ever
// broadcast. PanelManager:TogglePanel against one of these per-action
// panels is the supported way to turn a click into a UiMessage this plugin
// can react to.
Panels::RegistrationHandle g_actionAutoSortPanel          = Panels::InvalidHandle;
Panels::RegistrationHandle g_actionAddBlankPagePanel      = Panels::InvalidHandle;
Panels::RegistrationHandle g_actionRemoveCurrentPagePanel = Panels::InvalidHandle;
// Single-grid Sort buttons (see grid_sort.hpp): same trigger-panel trick.
Panels::RegistrationHandle g_actionSortPersonalStashPanel = Panels::InvalidHandle;
Panels::RegistrationHandle g_actionSortInventoryPanel     = Panels::InvalidHandle;
Panels::RegistrationHandle g_actionSortCubePanel          = Panels::InvalidHandle;

// The inventory's Sort button is a plugin child layout on the stock
// PlayerInventory panel (the one place the SDK lets a plugin add UI to a
// stock panel -- see ui_layouts.hpp). The personal-stash and cube buttons
// live in the mod's own layouts instead (tools/patch_mod_stash_layout.py).
Panels::ChildLayoutHandle g_inventoryToolsChildLayout = Panels::InvalidChildLayoutHandle;

// The personal stash's Sort button sits exactly under the shared stash's Auto
// Sort button (tools/patch_mod_stash_layout.py: D2R has no container that only
// the Personal tab shows, so on the Shared tab Auto Sort covers it). If one
// click ever reaches both, DoSortGrid drops the personal sort: these are the
// UI-thread click times, in steady_clock milliseconds, compared there.
std::atomic<int64_t> g_lastAutoSortClickMs { -1000000 };
std::atomic<int64_t> g_lastPersonalSortClickMs { -1000000 };
constexpr int64_t    kSameClickWindowMs = 500;

int64_t NowMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The SDK has no query for "which shared-stash tab is on screen right now"
// or "how many active shared-stash tabs exist" (see README "Known gaps").
// v1 works around the first gap by having the plugin track its own
// "selected tab" pointer (moved with the < Tab / Tab > buttons, shown in the
// toast) instead of the tab D2R itself has open; the second gap is worked
// around with the shared_stash_tab_count config value below, which the user
// must set to match their actual stash.
std::atomic<uint32_t> g_selectedTab { 0 };

// Whether g_selectedTab is known to match the shared-stash page D2R is
// showing right now. The SDK cannot read the page directly (WidgetService has
// no getter for BankExpansionLayout's CurrentSharedStashText -- getInputText
// returns Unsupported on a TextBoxWidget, verified live), so the page is
// tracked from events instead:
//   - ItemTooltipEvent on a shared-stash item: sets the page exactly
//     (ItemInfo::sharedStashPage of the hovered item);
//   - BankPanelMessage:SharedStashRight / SharedStashLeft UiMessages (the
//     native page arrows, verified live): +1 / -1 from a known page;
//   - PanelManager:OpenPanel:BankExpansionLayout (stash opened): unknown
//     again until the next hover, since D2R may not reopen on the same page.
// Add Blank Page / Remove Current Page refuse to run while this is false.
std::atomic<bool> g_selectedTabKnown { false };

// add_page_enabled / remove_page_enabled from the config. false hides the
// matching button in the shared-stash strip (WidgetService::setWidgetVisible
// on the widget the mod layout patch inserted -- the buttons live in the
// mod's own BankExpansionLayout, not in a plugin layout, so hiding them at
// runtime is the only way to make this a setting instead of a re-patch) and
// also refuses the action, in case hiding failed. Re-read from the config
// file every time the stash opens, so no game restart is needed. Atomics:
// written on the UI thread, read on the UI thread only today, but kept
// atomic like g_selectedTab in case an action handler ever checks them from
// the game thread.
std::atomic<bool> g_addPageEnabled { true };
std::atomic<bool> g_removePageEnabled { true };
// [item-sorting.shared_stash] enabled: false hides all three
// shared-stash buttons and ignores their actions. Same refresh path as the two
// flags above; the game-thread actions also re-check it from a fresh read.
std::atomic<bool> g_sharedStashEnabled { true };
SharedEvents::ListenerHandle g_tooltipListener = SharedEvents::InvalidHandle;

struct Config {
	// [item-sorting] enabled: the whole plugin. Read once at load;
	// false = register nothing (see D2RLoaderLoadPlugin).
	bool                                        enabled    = true;
	// Everything from here to `layout` (and addPageEnabled/removePageEnabled,
	// startTab..addEmptyPageWhen below) is [item-sorting.shared_stash],
	// re-read on every shared-stash action (RefreshSharedStashConfig).
	bool                                        sharedEnabled = true;
	uint32_t                                    gridWidth  = 16;
	uint32_t                                    gridHeight = 13;
	uint32_t                                    tabCount   = 20;
	std::vector<ItemSorting::CategorySpec> layout;
	// Debug aid for the "Known gaps" #6/#7 live-layout work: when true, every
	// incoming UiMessageEvent (ours and every other plugin's/D2R's own) is
	// logged as target/command, so the exact native message D2R fires when
	// the Shared Stash panel opens/closes/switches tabs can be found by
	// watching the log while toggling it in-game. Off by default -- this is
	// a firehose, not something to leave on.
	bool                                        debugLogUiMessages = false;

	// Show + allow the shared-stash Add Page / Remove Page buttons. See
	// g_addPageEnabled above.
	bool                                        addPageEnabled    = true;
	bool                                        removePageEnabled = true;

	// Mirrors the save-editor's autosort.json layer (startTab,
	// blankTabsAtStart, compactEmptyTabsFirst, addEmptyPageWhen) -- see
	// sort_engine.hpp's AutoSortConfig comment for exactly what is and isn't
	// ported (the quota-shortfall "padding" pass is not -- it doesn't apply
	// to how this engine reserves tabs).
	uint32_t                                    startTab              = 0;
	uint32_t                                    blankTabsAtStart      = 3;
	bool                                        compactEmptyTabsFirst = true;
	ItemSorting::EmptyPageRule           addEmptyPageWhen      = ItemSorting::EmptyPageRule::AtPercent(30.0);

	// Empty (the default) means "use the embedded, build-time-generated
	// item_classification_data.hpp table". Non-empty is a mod folder (e.g.
	// "D:\...\mods\Reimagined\Reimagined.mpq") whose OWN
	// data/global/excel/{itemtypes,weapons,armor,misc,setitems}.txt are
	// parsed fresh (see mod_item_data.hpp) so item classification always
	// matches whichever mod is actually active, instead of a bundled
	// reference copy that can silently drift out of sync with it (see
	// README "Known gaps" for the concrete bug this was added to fix).
	std::string                                 modDataPath;

	// The three single-grid containers (personal stash, inventory, cube):
	// each section already parsed and range-checked by grid_config_toml.hpp.
	// A container with parse errors refuses to sort (see DoSortGrid).
	ItemSorting::Grid::ParsedSection personalStash;
	ItemSorting::Grid::ParsedSection inventory;
	ItemSorting::Grid::ParsedSection cube;
	// Set when the file exists but isn't valid TOML at all. The shared stash
	// keeps its old behavior (built-in defaults); the single-grid sorts
	// refuse to run instead, since the user clearly meant to configure them.
	std::string                                 fileError;
};

Config g_config;

// "never" | "always" | 0-100 (any TOML number type) -> EmptyPageRule. Mirrors
// _ssNormEmptyPageRule. Returns false (leaving *outRule untouched) when the
// node is absent or its value doesn't parse, so callers can tell "not set,
// inherit the global/default setting" apart from "set to an invalid value".
template <typename Node>
bool TryParseEmptyPageRule(toml::node_view<Node> node, ItemSorting::EmptyPageRule* outRule) {
	if (!node) {
		return false;
	}
	if (const auto* s = node.as_string()) {
		if (s->get() == "never") {
			*outRule = ItemSorting::EmptyPageRule::Never();
			return true;
		}
		if (s->get() == "always") {
			*outRule = ItemSorting::EmptyPageRule::AtPercent(0.0);
			return true;
		}
		return false;
	}
	if (auto d = node.template value<double>()) {
		if (*d >= 0.0 && *d <= 100.0) {
			*outRule = ItemSorting::EmptyPageRule::AtPercent(*d);
			return true;
		}
	}
	return false;
}

// A TOML array of strings -> vector<string>, e.g. a [[layout]] entry's
// sort_keys or gear_groups. Missing, not-an-array, or an array with
// non-string entries all just yield an empty vector (CategorySpec then falls
// back to its mode's own built-in default -- see sort_engine.cpp), same
// "degrade gracefully" spirit as TryParseEmptyPageRule above.
template <typename Node>
std::vector<std::string> ParseStringArray(toml::node_view<Node> node) {
	std::vector<std::string> out;
	if (const auto* arr = node.as_array()) {
		out.reserve(arr->size());
		for (auto&& v : *arr) {
			if (auto s = v.template value<std::string>()) {
				out.push_back(std::move(*s));
			}
		}
	}
	return out;
}

// Reads the plugin's TOML config file into *out. Logs and returns false on
// failure (callers then fall back to built-in defaults).
bool ReadConfigText(const PluginContext* ctx, std::string* out) {
	// The shipped file is ~32 KB since the single-grid sections were added;
	// grow the buffer to whatever ReadConfig says it needs rather than
	// assuming any fixed size.
	std::vector<char> buffer(64 * 1024, '\0');
	uint32_t          required = 0;
	bool              readOk   = ctx->ReadConfig(buffer.data(), static_cast<uint32_t>(buffer.size()), &required);
	if (required > buffer.size()) {
		buffer.assign(static_cast<size_t>(required) + 1, '\0');
		readOk = ctx->ReadConfig(buffer.data(), static_cast<uint32_t>(buffer.size()), &required);
	}
	if (!readOk) {
		ctx->LogWarn("[config] could not read quyen19492-item-sorting.toml; using built-in defaults.");
		return false;
	}
	if (required > buffer.size()) {
		ctx->LogWarn("[config] config file is larger than expected; using built-in defaults.");
		return false;
	}

	out->assign(buffer.data(), buffer.data() + required);
	while (!out->empty() && out->back() == '\0') {
		out->pop_back();
	}
	return true;
}

Config LoadConfig(const PluginContext* ctx) {
	Config cfg;
	cfg.layout        = ItemSorting::DefaultLayout();
	cfg.personalStash.config = ItemSorting::Grid::DefaultConfig("personal_stash");
	cfg.inventory.config     = ItemSorting::Grid::DefaultConfig("inventory");
	cfg.cube.config          = ItemSorting::Grid::DefaultConfig("cube");
	if (ctx == nullptr) {
		return cfg;
	}

	std::string text;
	if (!ReadConfigText(ctx, &text)) {
		return cfg;
	}

	// toml++ compiles in "exceptions" mode whenever the toolchain has C++
	// exceptions enabled (MSVC's default /EHsc), which is the case for this
	// plugin. In that mode toml::parse_result is just an alias for
	// toml::table and toml::parse() throws toml::parse_error on failure
	// instead of returning a checkable result object.
	toml::table parsed;
	try {
		parsed = toml::parse(text);
	} catch (const toml::parse_error& err) {
		const std::string msg = std::string("[config] config parse error: ") + std::string(err.description());
		ctx->LogWarn(msg.c_str());
		cfg.fileError = std::string(err.description()) + " (line " + std::to_string(err.source().begin.line) + ")";
		return cfg;
	}

	cfg.personalStash = ItemSorting::Grid::ParseGridSection(parsed, "personal_stash");
	cfg.inventory     = ItemSorting::Grid::ParseGridSection(parsed, "inventory");
	cfg.cube          = ItemSorting::Grid::ParseGridSection(parsed, "cube");

	// The sections were called [quyen194-item-sorting...] (the plugin's id at
	// the time) before; an old file would otherwise silently run on built-in
	// defaults.
	if (parsed["quyen194-item-sorting"] && !parsed["item-sorting"]) {
		ctx->LogWarn(
			"[config] the config file still uses the old [quyen194-item-sorting...] section names and is ignored -- rename them to "
			"[item-sorting], [item-sorting.shared_stash], ... (see the shipped quyen19492-item-sorting.toml). Using built-in defaults.");
	}

	// [item-sorting]: plugin-wide settings only.
	auto section = parsed["item-sorting"];
	cfg.enabled            = section["enabled"].value_or(true);
	cfg.debugLogUiMessages = section["debug_log_ui_messages"].value_or(false);

	// Keys that used to live directly under [item-sorting] before the
	// shared stash got its own section. An older config file would otherwise
	// silently run on defaults, so say so.
	for (const char* legacy : { "shared_stash_tab_count", "start_tab", "blank_tabs_at_start", "compact_empty_tabs_first", "add_empty_page_when",
			 "add_page_enabled", "remove_page_enabled", "grid", "layout" }) {
		if (section[legacy]) {
			ctx->LogWarn((std::string("[config] '") + legacy
						  + "' directly under [item-sorting] is ignored -- shared-stash settings now live in "
							"[item-sorting.shared_stash] (grid.width/height became grid_width/grid_height, [[layout]] became "
							"[[item-sorting.shared_stash.layout]]).")
							 .c_str());
		}
	}

	// [item-sorting.shared_stash]
	auto shared = section["shared_stash"];
	if (!shared.as_table()) {
		ctx->LogWarn("[config] [item-sorting.shared_stash] not found; the shared stash uses built-in defaults.");
	}
	auto gridSide = [&](const char* key, uint32_t fallback) -> uint32_t {
		auto node = shared[key];
		if (!node) return fallback;
		auto v = node.value<int64_t>();
		if (node.is_integer() && v && *v >= 1 && *v <= 16) return static_cast<uint32_t>(*v);
		ctx->LogWarn((std::string("[config] [shared_stash] ") + key + " must be an integer 1-16; using " + std::to_string(fallback) + ".")
						 .c_str());
		return fallback;
	};
	cfg.sharedEnabled      = shared["enabled"].value_or(true);
	cfg.tabCount           = shared["shared_stash_tab_count"].value_or(20u);
	cfg.gridWidth          = gridSide("grid_width", 16u);
	cfg.gridHeight         = gridSide("grid_height", 13u);
	cfg.addPageEnabled     = shared["add_page_enabled"].value_or(true);
	cfg.removePageEnabled  = shared["remove_page_enabled"].value_or(true);

	cfg.startTab              = shared["start_tab"].value_or(0u);
	cfg.blankTabsAtStart      = shared["blank_tabs_at_start"].value_or(3u);
	cfg.compactEmptyTabsFirst = shared["compact_empty_tabs_first"].value_or(true);
	{
		auto node = shared["add_empty_page_when"];
		ItemSorting::EmptyPageRule rule;
		if (TryParseEmptyPageRule(node, &rule)) {
			cfg.addEmptyPageWhen = rule;
		} else if (node) {
			ctx->LogWarn(
				"[config] add_empty_page_when must be \"never\", \"always\", or a number 0-100; using the built-in "
				"default (30).");
		}
	}

	if (auto layoutArr = shared["layout"].as_array()) {
		std::vector<ItemSorting::CategorySpec> parsedLayout;
		bool                                           allKnown = true;
		for (auto&& node : *layoutArr) {
			const auto* row = node.as_table();
			if (row == nullptr) {
				continue;
			}
			std::string modeId  = (*row)["mode_id"].value_or(std::string {});
			uint32_t    minTabs = (*row)["min_tabs"].value_or(0u);
			if (modeId.empty() || !ItemSorting::IsKnownModeId(modeId)) {
				allKnown = false;
				break;
			}

			ItemSorting::CategorySpec spec;
			spec.id         = modeId;
			spec.minTabs    = minTabs;
			spec.sortKeys   = ParseStringArray((*row)["sort_keys"]);
			spec.gearGroups = ParseStringArray((*row)["gear_groups"]);

			auto                               overrideNode = (*row)["add_empty_page_when"];
			ItemSorting::EmptyPageRule catRule;
			if (TryParseEmptyPageRule(overrideNode, &catRule)) {
				spec.hasEmptyPageOverride = true;
				spec.emptyPageOverride    = catRule;
			} else if (overrideNode) {
				ctx->LogWarn(
					("[config] [[layout]] entry '" + modeId
					 + "' has an invalid add_empty_page_when; inheriting the global setting.")
						.c_str());
			}

			parsedLayout.push_back(std::move(spec));
		}
		if (allKnown && !parsedLayout.empty()) {
			cfg.layout = std::move(parsedLayout);
		} else if (!allKnown) {
			ctx->LogWarn("[config] config has an unknown mode_id in [[shared_stash.layout]]; using the built-in default layout instead.");
		}
	}

	cfg.modDataPath = section["mod_data_path"].value_or(std::string {});

	// Switch the item-classification table (item_classification.cpp) to
	// match: empty path -> embedded table, non-empty -> parse that mod's own
	// txt files fresh. This re-parses on every LoadConfig call (i.e. every
	// Auto Sort click, same as the rest of hot-reload) rather than caching
	// by path, matching this plugin's existing "just re-read everything"
	// hot-reload philosophy -- the txt files involved are a few hundred KB
	// total, not a per-frame cost.
	if (cfg.modDataPath.empty()) {
		ItemSorting::Classification::UseEmbeddedTable();
	} else {
		std::string loadError;
		if (!ItemSorting::Classification::LoadFromModFolder(cfg.modDataPath, &loadError)) {
			ctx->LogWarn(
				("[config] mod_data_path is set to '" + cfg.modDataPath + "' but could not be used (" + loadError
				 + "); falling back to the embedded classification table.")
					.c_str());
			// LoadFromModFolder already reverted to the embedded table on failure.
		}
	}

	return cfg;
}

// --- Log tags ----------------------------------------------------------------
// D2RLoader already prefixes every line with the plugin id, so each message
// starts with the feature it is about instead: [remote-stash] (shared stash),
// [personal-stash], [inventory], [cube], plus [config] and [plugin] for lines
// that belong to none of them.

const char* LogTagFor(Items::ItemContainer container) {
	switch (container) {
		case Items::ItemContainer::PersonalStash: return "[personal-stash] ";
		case Items::ItemContainer::Inventory: return "[inventory] ";
		case Items::ItemContainer::Cube: return "[cube] ";
		default: return "[remote-stash] ";
	}
}

// grid_sort.cpp starts its own log/error lines with "[<section>] " (e.g.
// "[personal_stash] "); drop that, the log tag already says which grid.
std::string WithoutSectionTag(const std::string& line, const char* section) {
	const std::string prefix = std::string("[") + section + "] ";
	return line.rfind(prefix, 0) == 0 ? line.substr(prefix.size()) : line;
}

// --- Reading the live shared stash into ItemFacts --------------------------

// Shared by the set-group diagnostic below and the move-rejection log
// further down (ApplyMoves) -- unpacks a D2RL::Items::MakeItemCode-style
// little-endian uint32 back into its up-to-4-char text form for logging.
std::string UnpackItemCode(uint32_t packedCode) {
	char text[5] {};
	for (int i = 0; i < 4; ++i) {
		text[i] = static_cast<char>((packedCode >> (8 * i)) & 0xFF);
	}
	std::string result(text);
	while (!result.empty() && result.back() == ' ') {
		result.pop_back();
	}
	return result;
}

struct EnumState {
	std::vector<ItemSorting::ItemFacts> facts;

	// Classification coverage diagnostic (README "Known gaps" -- the
	// generated item_classification_data.hpp table is only as good as its
	// source *.txt snapshot). A code that fails to classify falls back to
	// ItemFacts{} defaults (isGear=false, w=h=1), which silently excludes it
	// from every auto-sort category *and* silently under-marks its true
	// footprint if some other item's classification is what's wrong instead.
	size_t                classifiedCount   = 0;
	size_t                unclassifiedCount = 0;
	std::vector<uint32_t> sampleUnclassifiedCodes;

	// Which container this scan collects. SharedStash for the shared-stash
	// buttons; PersonalStash / Inventory / Cube for the single-grid Sort
	// buttons (sharedStashPage is then forced to 0 -- see below).
	Items::ItemContainer container = Items::ItemContainer::SharedStash;

	// Single-grid diagnostic: how many scanned items reported each
	// ItemInfo::inventoryPage value. Unverified live: whether a mod's extra
	// stash tabs (Reimagined's Gems / Materials / Runes pages) report their
	// items as PersonalStash with some other page number -- this is what the
	// first live run needs to show.
	std::unordered_map<int32_t, size_t> itemsByInventoryPage;
};

auto __cdecl CollectSharedStashItem(const PluginContext* ctx, const Items::ItemInfo* item, void* userData) noexcept -> Inventory::IterationAction {
	auto* state = static_cast<EnumState*>(userData);
	if (state == nullptr || item == nullptr || item->structSize < Items::ItemInfoRequiredSize) {
		return Inventory::IterationAction::Continue;
	}
	if (item->container != state->container) {
		return Inventory::IterationAction::Continue;
	}

	ItemSorting::ItemFacts fact;
	fact.handle          = item->handle;
	fact.code            = item->code;
	fact.quality         = static_cast<uint32_t>(item->quality);
	fact.itemLevel       = item->itemLevel;
	fact.sharedStashPage = item->sharedStashPage;
	fact.x               = item->x;
	fact.y               = item->y;

	uint32_t baseLevelReq = 0;
	if (auto cls = ItemSorting::Classification::Classify(item->code)) {
		fact.w           = cls->width;
		fact.h           = cls->height;
		fact.isGear       = cls->isGear;
		fact.isTwoHanded  = cls->isTwoHanded;
		fact.gearSlot     = cls->gearSlot;
		fact.nonGearKind  = cls->nonGearKind;
		baseLevelReq      = cls->baseLevelReq;
		++state->classifiedCount;
	} else {
		++state->unclassifiedCount;
		if (state->sampleUnclassifiedCodes.size() < 12) {
			state->sampleUnclassifiedCodes.push_back(item->code);
		}
	}

	// misc.txt kind (rune / gem / potion / tool / quest) for the single-grid
	// categories. Also the only source of a footprint for gems and runes,
	// which Classify() deliberately leaves out (always 1x1 in practice, same
	// as the ItemFacts default -- so the shared stash sees no change).
	if (auto misc = ItemSorting::Classification::ClassifyMisc(item->code)) {
		fact.miscKind  = misc->kind;
		fact.miscOrder = misc->order;
		fact.miscGrade = misc->grade;
		if (!ItemSorting::Classification::Classify(item->code)) {
			fact.w = misc->width;
			fact.h = misc->height;
		}
	}

	if (state->container != Items::ItemContainer::SharedStash) {
		// One grid: grid_sort.cpp and the move code treat it as "tab 0".
		fact.sharedStashPage = 0;
		++state->itemsByInventoryPage[item->inventoryPage];
	}

	// TrueReqLevel() is a full port of the save-editor's _ssItemSortLevel: it
	// raises baseLevelReq using UniqueItems.txt/SetItems.txt's own "lvl req"
	// (qualityRecordId is a valid row for BOTH Unique and Set quality -- see
	// item.h) and the highest "levelreq" among this item's rolled affixes
	// (prefixIds/suffixIds). This matters even for an unclassified code
	// (baseLevelReq==0 above): a Unique/Set/affix bump can still apply. Fixes
	// a real, previously-unnoticed gap: Rings.txt/Charms.txt/Jewels.txt/
	// Amulets.txt each have exactly one row with levelreq==0, so the
	// "reqLevel" sort key used to never vary across an entire ring/charm/
	// jewel/amulet category, regardless of quality or roll -- ties were
	// silently decided by whatever key came next (usually "-ilvl"), with no
	// relationship at all to the item's own displayed "Required Level".
	fact.sortLevel = ItemSorting::Classification::TrueReqLevel(fact.quality, item->qualityRecordId, item->prefixIds, item->suffixIds, baseLevelReq);

	if (item->quality == Items::Quality::Set) {
		fact.setId        = item->qualityRecordId;
		fact.setGroupName = ItemSorting::Classification::SetGroupForRow(item->qualityRecordId);

		// Diagnostic for a live report: a specific set item (a Grim Helm,
		// qualityRecordId pointing at "Natalya's Odium") never got moved by
		// Auto Sort despite the "set" category clearly running and placing
		// hundreds of *other* set items successfully. item_classification.hpp's
		// kSetGroupByRow table (item_classification_data.hpp) DOES contain 4
		// rows for "Natalya's Odium", so the table isn't simply missing that
		// set's name -- but SetGroupForRow() also returns "" when
		// qualityRecordId is negative or falls outside the table's own
		// [0, 456) row range, and sort_engine.cpp's "set" mode treats an empty
		// setGroupName as "same as a completely unknown item": left in place,
		// forever, with no warning and no move-rejection log line either (it
		// is simply never added to any category's plan). Logging the raw
		// qualityRecordId here -- for every Set-quality item whose lookup came
		// back empty -- is what will finally show whether this is an
		// out-of-range/off-by-one index into kSetGroupByRow (a genuine data
		// gap in that generated table) or something else entirely (e.g. a
		// qualityRecordId the live SDK assigns that this table's row layout
		// was never built to represent). Not yet confirmed -- see README
		// "Known gaps" #3.
		if (fact.setGroupName.empty() && ctx != nullptr) {
			char msg[224];
			std::snprintf(
				msg, sizeof(msg),
				"%sset item handle %llu (code='%s') has qualityRecordId=%d, which "
				"could not be mapped to a set name; Auto Sort will leave it exactly where it is (see "
				"README Known gaps #3).",
				LogTagFor(state->container), static_cast<unsigned long long>(item->handle), UnpackItemCode(item->code).c_str(),
				item->qualityRecordId);
			ctx->LogWarn(msg);
		}
	}

	state->facts.push_back(std::move(fact));
	return Inventory::IterationAction::Continue;
}

auto CollectSharedStash(const PluginContext* ctx, PlayerHandle player, EnumState* state) -> bool {
	const Inventory::ItemFilter filter {
		.structSize    = Inventory::ItemFilterSize,
		.containerMask = Items::ContainerBit(Items::ItemContainer::SharedStash),
	};
	const bool ok = g_inventory->forEachInventoryItem(ctx, player, &filter, CollectSharedStashItem, state) == Inventory::Result::Success;

	// Always-on diagnostic for "Known gaps" #8 -- the single biggest
	// unverified assumption this whole plugin rests on: that
	// forEachInventoryItem + ItemContainer::SharedStash enumerates every
	// tab (disambiguated via ItemInfo::sharedStashPage), not just whichever
	// tab is currently open on screen. Cheap (only runs when a button is
	// clicked, not per-frame), so it stays on unconditionally rather than
	// behind debug_log_ui_messages.
	if (ok && ctx != nullptr && state != nullptr) {
		std::vector<uint32_t> tabs;
		tabs.reserve(state->facts.size());
		for (const auto& fact : state->facts) {
			tabs.push_back(fact.sharedStashPage);
		}
		std::sort(tabs.begin(), tabs.end());
		tabs.erase(std::unique(tabs.begin(), tabs.end()), tabs.end());

		std::string tabList;
		for (size_t i = 0; i < tabs.size() && i < 40; ++i) {
			if (!tabList.empty()) {
				tabList += ",";
			}
			tabList += std::to_string(tabs[i]);
		}
		if (tabs.size() > 40) {
			tabList += ",...";
		}

		char buf[512];
		std::snprintf(
			buf,
			sizeof(buf),
			"[remote-stash] shared-stash scan: %zu item(s) across %zu distinct tab number(s) [%s]. If this "
			"stays the SAME single tab number no matter which tab is open in-game when you click a button, "
			"forEachInventoryItem is only returning the currently open tab, not every tab (see README Known gaps #8).",
			state->facts.size(),
			tabs.size(),
			tabList.c_str());
		ctx->LogInfo(buf);

		// Classification coverage: how many of the scanned items resolved
		// against item_classification_data.hpp vs fell back to defaults
		// (isGear=false, w=h=1 -- see EnumState comment). A high miss rate,
		// or misses concentrated on codes that should obviously be gear,
		// points at the generated table being stale for this mod version
		// rather than a bug in RunAutoSort/ApplyMoves themselves.
		std::string sampleCodes;
		for (uint32_t code : state->sampleUnclassifiedCodes) {
			char text4[5] {
				static_cast<char>(code & 0xFF), static_cast<char>((code >> 8) & 0xFF), static_cast<char>((code >> 16) & 0xFF),
				static_cast<char>((code >> 24) & 0xFF), 0
			};
			for (char& c : text4) {
				if (c != 0 && (c < 0x20 || c > 0x7E)) c = '?';
			}
			if (!sampleCodes.empty()) sampleCodes += ",";
			sampleCodes += "'";
			sampleCodes += text4;
			sampleCodes += "'";
		}
		char classBuf[512];
		std::snprintf(
			classBuf,
			sizeof(classBuf),
			"[remote-stash] item classification: %zu matched, %zu unmatched (defaults used -- excluded from "
			"every category) out of %zu scanned. Sample unmatched codes: [%s]",
			state->classifiedCount,
			state->unclassifiedCount,
			state->facts.size(),
			sampleCodes.c_str());
		ctx->LogInfo(classBuf);
	}

	return ok;
}

// --- Turning a plan into an item transaction --------------------------------

// Unpacks a D2RL::Items::MakeItemCode-style packed code back into up to 4
// printable characters (space-padded codes trim their trailing spaces), for
// diagnostic logging only -- see the rejection log below. Added after an
// offline replay of a live run's data (same classification table, same
// config) pointed at a pattern worth confirming directly: every rejection
// so far has landed on an item the "unclassified" catch-all category placed
// (a potion, gem, rune, or quest item -- see README "Known gaps" #3), never
// on ordinary gear/charm/jewel/ring/amulet. This line is what the next real
// run needs to confirm or rule that out for good, instead of inferring it
// from tab numbers after the fact. (UnpackItemCode itself now lives above,
// next to CollectSharedStashItem, since the set-group diagnostic there needs
// it too.)

auto ApplyMoves(
	const PluginContext* ctx, PlayerHandle player, const ItemSorting::SequencedMoves& sequenced,
	const std::vector<ItemSorting::ItemFacts>& items, uint32_t* committedCount) -> Items::Result {
	if (committedCount != nullptr) {
		*committedCount = 0;
	}
	const auto& moves = sequenced.moves;
	if (moves.empty()) {
		return Items::Result::Success;
	}
	if (sequenced.unsafe) {
		// See SequencedMoves::unsafe: never let the game see a move onto an
		// occupied cell -- that is how the single-grid sort lost items.
		for (const auto& note : sequenced.diagnostics) {
			ctx->LogWarn(("[remote-stash] move ordering: " + note).c_str());
		}
		ctx->LogWarn(
			"[remote-stash] no safe move order exists for this plan (a dependency cycle has no free scratch cell); refusing to "
			"apply it. Nothing was moved.");
		return Items::Result::Conflict;
	}

	// Handle -> facts, purely for the rejection log below (what code/category
	// was this?) -- never consulted for planning, which already happened.
	std::unordered_map<uint64_t, const ItemSorting::ItemFacts*> factsByHandle;
	factsByHandle.reserve(items.size());
	for (const auto& it : items) factsByHandle.emplace(it.handle, &it);

	for (const auto& note : sequenced.diagnostics) {
		const std::string msg = "[remote-stash] move ordering: " + note;
		ctx->LogWarn(msg.c_str());
	}

	std::vector<Items::ExistingItemOperation> ops;
	ops.reserve(moves.size());
	for (const auto& mv : moves) {
		Items::ExistingItemOperation op {};
		op.structSize                       = Items::ExistingItemOperationSize;
		op.kind                             = Items::ExistingItemOperationKind::Move;
		op.item                             = static_cast<ItemHandle>(mv.handle);
		op.move.destination.structSize      = Items::ItemDestinationSize;
		op.move.destination.container       = Items::ItemContainer::SharedStash;
		op.move.destination.placement       = Items::Placement::Exact;
		op.move.destination.x               = static_cast<uint32_t>(mv.destX);
		op.move.destination.y               = static_cast<uint32_t>(mv.destY);
		op.move.destination.sharedStashPage = mv.destTab;
		ops.push_back(op);
	}

	uint32_t      totalCommitted = 0;
	Items::Result worstResult    = Items::Result::Success; // first non-Success result seen, if any

	// A single call to executeExistingItemTransaction is all-or-nothing (see
	// ExistingItemTransactionResult's own doc comment: "committedOperationCount
	// is zero on failure") -- so one operation this API rejects (e.g. an item
	// it doesn't consider movable from wherever it actually is) sinks every
	// other, otherwise-fine operation riding in the same call with it.
	// `attempt` isolates it instead: on any non-Success result it bisects the
	// failing slice and retries each half separately, recursing down to
	// individual operations when necessary, logging exactly which item and
	// why once isolated to one, so the rest of a chunk still commits
	// everything it safely can. Each Move destination is idempotent (an
	// exact tab/x/y), so retrying a slice that may have partially applied
	// despite reporting failure is harmless either way.
	//
	// `sequenced` is grouped into dependency LAYERS (see
	// SequenceMovesForLiveApply/SequencedMoves::layerSizes in
	// sort_engine.hpp): every move in one layer is independent of every
	// other move in that same layer, so bisecting/chunking freely WITHIN a
	// layer is always safe -- but a layer must fully finish (whatever of it
	// can commit, has) before the NEXT layer's calls start, since later
	// layers depend on earlier ones having actually happened live. `attempt`
	// itself doesn't need to know about layers at all; the outer loop below
	// just never lets a `begin,end` range cross a layer boundary.
	std::function<void(size_t, size_t)> attempt = [&](size_t begin, size_t end) {
		if (begin >= end) {
			return;
		}

		Items::ExistingItemTransaction txn {};
		txn.structSize     = Items::ExistingItemTransactionSize;
		txn.player         = player;
		txn.operationCount = static_cast<uint32_t>(end - begin);
		txn.operations     = ops.data() + begin;

		Items::ExistingItemTransactionResult result {};
		result.structSize = Items::ExistingItemTransactionResultSize;

		const auto txnResult = g_items->executeExistingItemTransaction(ctx, &txn, &result);
		if (txnResult == Items::Result::Success) {
			totalCommitted += result.committedOperationCount;
			return;
		}

		if (worstResult == Items::Result::Success) {
			worstResult = txnResult; // remember the first failure code seen
		}

		if (end - begin == 1) {
			const auto& op = ops[begin];

			// What kind of item was this? (code/category, not just a bare
			// handle) -- added after a live run's rejections turned out to
			// cluster entirely on Misc.txt-classified items (potions, gems,
			// runes, quest items) landing in the "unclassified" catch-all
			// category, not on anything about the destination tab. See
			// README "Known gaps" #3.
			std::string category = "?";
			std::string codeText;
			auto        factIt = factsByHandle.find(static_cast<uint64_t>(op.item));
			if (factIt != factsByHandle.end()) {
				const auto* fact = factIt->second;
				codeText          = UnpackItemCode(fact->code);
				if (fact->isGear) {
					category = "gear(slot=" + std::to_string(static_cast<int>(fact->gearSlot)) + ")";
				} else {
					switch (fact->nonGearKind) {
						case ItemSorting::NonGearKind::Charm: category = "charm"; break;
						case ItemSorting::NonGearKind::Jewel: category = "jewel"; break;
						case ItemSorting::NonGearKind::Ring: category = "ring"; break;
						case ItemSorting::NonGearKind::Amulet: category = "amulet"; break;
						default: category = "unclassified(misc/gem/rune/quest/potion)"; break;
					}
				}
			}

			// Is this a real destination, or a synthetic "move to scratch cell"
			// this run's dependency-cycle break inserted (see
			// SequencedMoves::isScratchMove)? A live report of rejections
			// clustering on specific, otherwise-empty tab numbers turned out
			// to be exactly the scratch-cell case -- distinguishing it here
			// means the NEXT such report says so directly instead of needing
			// to be re-derived from which tabs the rest of the plan touches.
			const bool isScratch = begin < sequenced.isScratchMove.size() && sequenced.isScratchMove[begin];

			char msg[512];
			std::snprintf(
				msg, sizeof(msg),
				"[remote-stash] move for item handle %llu (code='%s', %s) -> tab %u (%u,%u) was rejected (result "
				"%u); skipping just this item, the rest of the plan is unaffected.%s",
				static_cast<unsigned long long>(op.item), codeText.c_str(), category.c_str(), op.move.destination.sharedStashPage,
				op.move.destination.x, op.move.destination.y, static_cast<unsigned>(txnResult),
				isScratch ? " [this was a temporary scratch-cell parking move to break a dependency cycle, not the item's real "
							"destination -- see README Known gaps #3/#8]"
						  : "");
			ctx->LogWarn(msg);
			return;
		}

		const size_t mid = begin + (end - begin) / 2;
		attempt(begin, mid);
		attempt(mid, end);
	};

	size_t layerOffset = 0;
	bool   loggedChunkWarning = false;
	for (uint32_t layerSize : sequenced.layerSizes) {
		const size_t layerEnd = layerOffset + layerSize;
		if (!loggedChunkWarning && layerSize > Items::MaxExistingItemOperations) {
			ctx->LogWarn(
				"[remote-stash] a single dependency layer exceeds 4096 operations; it will be sent in several "
				"transactions and is NOT atomic across all of them (see README).");
			loggedChunkWarning = true;
		}
		size_t offset = layerOffset;
		while (offset < layerEnd) {
			const size_t chunkSize = std::min<size_t>(Items::MaxExistingItemOperations, layerEnd - offset);
			attempt(offset, offset + chunkSize);
			offset += chunkSize;
		}
		layerOffset = layerEnd;
	}

	if (committedCount != nullptr) {
		*committedCount = totalCommitted;
	}
	return totalCommitted == ops.size() ? Items::Result::Success : worstResult;
}

// --- Toast -------------------------------------------------------------

void ShowToast(const PluginContext* ctx, ItemSorting::UiLayouts::OutcomeMessage message) {
	namespace UiLayouts = ItemSorting::UiLayouts;
	if (ctx == nullptr || g_panels == nullptr || g_widgets == nullptr) {
		return;
	}
	if (g_panels->openPanel(ctx, g_toastPanel) != Panels::Result::Success) {
		return;
	}

	Widgets::WidgetHandle panelWidget = Widgets::InvalidHandle;
	if (g_widgets->findPanel(ctx, "quyen19492-item-sorting/Toast", &panelWidget) != Widgets::Result::Success) {
		return;
	}

	for (uint32_t i = 0; i < static_cast<uint32_t>(UiLayouts::OutcomeMessage::Count); ++i) {
		const auto            candidate = static_cast<UiLayouts::OutcomeMessage>(i);
		Widgets::WidgetHandle w         = Widgets::InvalidHandle;
		if (g_widgets->findWidget(ctx, panelWidget, UiLayouts::OutcomeMessageWidgetName(candidate), &w) == Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(ctx, w, candidate == message);
		}
	}

	// The tab-number line shares screen space with the outcome-message text
	// above it (both TextBoxWidgets, overlapping rects by design -- see
	// ui_layouts.hpp), so it must only be shown for the TabSelected message;
	// showing it unconditionally made every toast (Auto Sort result, Add
	// Blank Page, etc.) render two overlapping lines of garbled text at once.
	const bool     showTabDisplay = (message == UiLayouts::OutcomeMessage::TabSelected);
	const uint32_t selected       = g_selectedTab.load();
	for (uint32_t t = 0; t <= UiLayouts::kMaxDisplayableTab + 1; ++t) {
		const bool        isOverflowSlot = (t == UiLayouts::kMaxDisplayableTab + 1);
		const std::string name           = isOverflowSlot ? std::string("MsgTabOverflow") : UiLayouts::TabWidgetName(t);
		const bool         shouldShow    = showTabDisplay && (isOverflowSlot ? (selected > UiLayouts::kMaxDisplayableTab) : (t == selected));

		Widgets::WidgetHandle w = Widgets::InvalidHandle;
		if (g_widgets->findWidget(ctx, panelWidget, name.c_str(), &w) == Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(ctx, w, shouldShow);
		}
	}
}

struct ToastRequest {
	ItemSorting::UiLayouts::OutcomeMessage message;
};

void __cdecl ShowToastOnUiThread(const PluginContext* ctx, void* userData) noexcept {
	std::unique_ptr<ToastRequest> owned(static_cast<ToastRequest*>(userData));
	if (owned == nullptr) {
		return;
	}
	ShowToast(ctx, owned->message);
}

// Queues the toast for the UI thread. Use this from a game-thread callback;
// call ShowToast(...) directly when already on the UI thread (the
// UiMessageListener callback below is UI-thread already).
void QueueToast(const PluginContext* ctx, ItemSorting::UiLayouts::OutcomeMessage message) {
	auto* req = new (std::nothrow) ToastRequest { message };
	if (req == nullptr) {
		return;
	}
	if (g_threads->runOnUiThread(ctx, ShowToastOnUiThread, req) != Threads::Result::Success) {
		delete req;
	}
}

// --- Button actions (run on the game thread) --------------------------

// Hot reload for the three shared-stash actions: re-read the config file and
// take the whole [item-sorting.shared_stash] section (the
// plugin-wide [item-sorting] block is load-time only). Runs on the
// game thread, like every action, so g_config is never read mid-update by
// another action. Returns the fresh shared_stash `enabled`.
bool RefreshSharedStashConfig(const PluginContext* ctx) {
	Config fresh                   = LoadConfig(ctx);
	g_config.sharedEnabled         = fresh.sharedEnabled;
	g_config.tabCount              = fresh.tabCount;
	g_config.gridWidth             = fresh.gridWidth;
	g_config.gridHeight            = fresh.gridHeight;
	g_config.addPageEnabled        = fresh.addPageEnabled;
	g_config.removePageEnabled     = fresh.removePageEnabled;
	g_config.layout                = std::move(fresh.layout);
	g_config.startTab              = fresh.startTab;
	g_config.blankTabsAtStart      = fresh.blankTabsAtStart;
	g_config.compactEmptyTabsFirst = fresh.compactEmptyTabsFirst;
	g_config.addEmptyPageWhen      = fresh.addEmptyPageWhen;
	g_sharedStashEnabled.store(fresh.sharedEnabled);
	g_addPageEnabled.store(fresh.addPageEnabled);
	g_removePageEnabled.store(fresh.removePageEnabled);
	if (!fresh.sharedEnabled) {
		ctx->LogInfo("[remote-stash] shared-stash action skipped -- [item-sorting.shared_stash] enabled = false.");
	}
	return fresh.sharedEnabled;
}

void __cdecl DoAutoSort(const PluginContext* ctx, void*) noexcept {
	namespace UiLayouts = ItemSorting::UiLayouts;

	if (!RefreshSharedStashConfig(ctx)) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::AutoSortDisabled);
		return;
	}

	PlayerHandle player = InvalidPlayerHandle;
	if (g_inventory->getLocalPlayer(ctx, &player) != Inventory::Result::Success) {
		ctx->LogWarn("[remote-stash] auto sort needs an authoritative local player.");
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	EnumState state;
	if (!CollectSharedStash(ctx, player, &state)) {
		ctx->LogWarn("[remote-stash] could not enumerate the shared stash.");
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	ItemSorting::AutoSortConfig sortCfg;
	sortCfg.layout                = g_config.layout;
	sortCfg.tabCount              = g_config.tabCount;
	sortCfg.gridWidth             = g_config.gridWidth;
	sortCfg.gridHeight            = g_config.gridHeight;
	sortCfg.startTab              = g_config.startTab;
	sortCfg.blankTabsAtStart      = g_config.blankTabsAtStart;
	sortCfg.compactEmptyTabsFirst = g_config.compactEmptyTabsFirst;
	sortCfg.addEmptyPageWhen      = g_config.addEmptyPageWhen;

	const auto outcome = ItemSorting::RunAutoSortWithPadding(state.facts, sortCfg);
	if (!outcome.ok) {
		const std::string logMsg = "[remote-stash] auto sort failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());

		UiLayouts::OutcomeMessage msg = UiLayouts::OutcomeMessage::InternalError;
		if (outcome.error.find("unknown mode_id") != std::string::npos) {
			msg = UiLayouts::OutcomeMessage::AutoSortUnknownMode;
		} else if (outcome.error.find("not enough shared-stash tabs") != std::string::npos) {
			msg = UiLayouts::OutcomeMessage::AutoSortNotEnoughTabs;
		} else if (outcome.error.find("set group") != std::string::npos) {
			msg = UiLayouts::OutcomeMessage::AutoSortSetNoRoom;
		} else if (outcome.error.find("needs more room") != std::string::npos) {
			msg = UiLayouts::OutcomeMessage::AutoSortNoRoom;
		}
		QueueToast(ctx, msg);
		return;
	}

	// Diagnostic: one line per category (tab span it actually used + how many
	// items landed there) so a "weird" in-game result can be checked against
	// what RunAutoSort actually planned, without guessing.
	for (const auto& line : outcome.categoryLog) {
		const std::string msg = "[remote-stash] plan: " + line;
		ctx->LogInfo(msg.c_str());
	}

	{
		char padMsg[256];
		std::snprintf(
			padMsg, sizeof(padMsg),
			"[remote-stash] compacted %u item(s) to relabel empty tabs to the end; added %u breathing page(s); pulled %u/%u "
			"blank tab(s) to the front.",
			outcome.compactMoveCount, outcome.breathingTabsAdded, outcome.blankTabsAtStartAdded, g_config.blankTabsAtStart);
		ctx->LogInfo(padMsg);
	}

	// Diagnostic: replay the plan against each item's live footprint and flag
	// any two moves that would land on the same cell(s). This tells apart a
	// planning-side bug (RunAutoSort / item classification data) from an
	// application-side one (ApplyMoves / the live transaction) when the
	// in-game result looks wrong -- see README "Known gaps".
	const auto collisions = ItemSorting::DetectCollisions(state.facts, outcome.moves, g_config.gridWidth, g_config.gridHeight);
	if (!collisions.empty()) {
		char summary[192];
		std::snprintf(
			summary, sizeof(summary), "[remote-stash] plan self-check found %zu colliding move(s) (showing up to 20):",
			collisions.size());
		ctx->LogWarn(summary);
		for (const auto& line : collisions) {
			const std::string msg = "[remote-stash] " + line;
			ctx->LogWarn(msg.c_str());
		}
	} else {
		ctx->LogInfo("[remote-stash] plan self-check found no colliding moves.");
	}

	// The plan above only guarantees a collision-free FINAL layout -- it says
	// nothing about the order moves have to happen in live. A full Auto Sort
	// repositions most of the stash, so most destinations are, at the moment
	// naive plan order would try to move into them, still occupied by some
	// OTHER item this same plan is about to move elsewhere; the live Move API
	// rejects that with Conflict no matter how ApplyMoves chunks/bisects,
	// since re-trying the SAME order never frees anything up. This resolves
	// that dependency order up front (see sort_engine.hpp's own comment for
	// the algorithm); ApplyMoves' own per-layer bisection stays as a backstop
	// for anything this couldn't foresee (stale facts, a genuinely
	// nonexistent tab, etc.), not the primary mechanism.
	// preferredMaxTab = outcome.tabsUsed: bias any dependency-cycle scratch
	// cell toward the tabs THIS plan actually places real items into, which
	// are proven safe for a live Move (see SequencedMoves::isScratchMove's
	// comment) -- rather than the bare `tabCount` padding above the real
	// stash size, which a live report traced a cluster of Unsupported
	// rejections back to.
	const auto sequenced = ItemSorting::SequenceMovesForLiveApply(
		state.facts, outcome.moves, g_config.tabCount, g_config.gridWidth, g_config.gridHeight, outcome.tabsUsed);

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, sequenced, state.facts, &committed);

	char logMsg[320] {};
	std::snprintf(
		logMsg,
		sizeof(logMsg),
		"[remote-stash] auto sort placed %u item(s) across %u tab(s); committed %u/%zu move op(s) (transaction result %u).",
		outcome.itemsSorted,
		outcome.tabsUsed,
		committed,
		sequenced.moves.size(),
		static_cast<unsigned>(txnResult));
	ctx->LogInfo(logMsg);

	if (txnResult == Items::Result::NotAuthoritative) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::MultiplayerClientUnsupported);
		return;
	}
	if (txnResult != Items::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}
	QueueToast(ctx, UiLayouts::OutcomeMessage::AutoSortOk);
}

void __cdecl DoAddBlankPage(const PluginContext* ctx, void*) noexcept {
	namespace UiLayouts = ItemSorting::UiLayouts;

	if (!RefreshSharedStashConfig(ctx)) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::AutoSortDisabled);
		return;
	}
	if (!g_config.addPageEnabled) {
		ctx->LogInfo("[remote-stash] Add Page is disabled (add_page_enabled = false); ignoring the click.");
		return;
	}

	PlayerHandle player = InvalidPlayerHandle;
	if (g_inventory->getLocalPlayer(ctx, &player) != Inventory::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	EnumState state;
	if (!CollectSharedStash(ctx, player, &state)) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	const auto outcome = ItemSorting::MoveBlankTabAfter(state.facts, g_selectedTab.load(), g_config.tabCount);
	if (!outcome.ok) {
		const std::string logMsg = "[remote-stash] add blank page after page " + std::to_string(g_selectedTab.load() + 1) + " failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::NoBlankTab);
		return;
	}

	// See DoAutoSort's own comment on SequenceMovesForLiveApply: a tab-shift
	// like this one moves a whole chain of tabs back one slot, and the same
	// "destination still occupied by another in-plan item" issue can apply
	// here too, just at tab granularity instead of per-cell.
	const auto sequenced =
		ItemSorting::SequenceMovesForLiveApply(state.facts, outcome.moves, g_config.tabCount, g_config.gridWidth, g_config.gridHeight);

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, sequenced, state.facts, &committed);
	if (txnResult == Items::Result::NotAuthoritative) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::MultiplayerClientUnsupported);
		return;
	}
	if (txnResult != Items::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}
	{
		char buf[200];
		std::snprintf(buf, sizeof(buf), "[remote-stash] add blank page after page %u: blank is now page %u; committed %u/%zu move(s).",
		               g_selectedTab.load() + 1, outcome.blankTab + 1, committed, sequenced.moves.size());
		ctx->LogInfo(buf);
	}
	QueueToast(ctx, UiLayouts::OutcomeMessage::BlankMoved);
}

void __cdecl DoRemoveCurrentPage(const PluginContext* ctx, void*) noexcept {
	namespace UiLayouts = ItemSorting::UiLayouts;

	if (!RefreshSharedStashConfig(ctx)) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::AutoSortDisabled);
		return;
	}
	if (!g_config.removePageEnabled) {
		ctx->LogInfo("[remote-stash] Remove Page is disabled (remove_page_enabled = false); ignoring the click.");
		return;
	}

	PlayerHandle player = InvalidPlayerHandle;
	if (g_inventory->getLocalPlayer(ctx, &player) != Inventory::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	EnumState state;
	if (!CollectSharedStash(ctx, player, &state)) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	const auto outcome = ItemSorting::PushBlankTabToEnd(state.facts, g_selectedTab.load(), g_config.tabCount);
	if (!outcome.ok) {
		const std::string logMsg = "[remote-stash] remove page " + std::to_string(g_selectedTab.load() + 1) + " failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());
		const auto msg = outcome.error.find("out of range") != std::string::npos ? UiLayouts::OutcomeMessage::OutOfRange : UiLayouts::OutcomeMessage::CurrentNotEmpty;
		QueueToast(ctx, msg);
		return;
	}

	// See DoAutoSort's own comment on SequenceMovesForLiveApply.
	const auto sequenced =
		ItemSorting::SequenceMovesForLiveApply(state.facts, outcome.moves, g_config.tabCount, g_config.gridWidth, g_config.gridHeight);

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, sequenced, state.facts, &committed);
	if (txnResult == Items::Result::NotAuthoritative) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::MultiplayerClientUnsupported);
		return;
	}
	if (txnResult != Items::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}
	{
		char buf[200];
		std::snprintf(buf, sizeof(buf), "[remote-stash] removed empty page %u (moved to the end); committed %u/%zu move(s).",
		               g_selectedTab.load() + 1, committed, sequenced.moves.size());
		ctx->LogInfo(buf);
	}
	QueueToast(ctx, UiLayouts::OutcomeMessage::PageRemoved);
}

// --- Single-grid Sort: personal stash / inventory / Horadric Cube -------------
// See grid_sort.hpp for the algorithm and the config file's single-grid
// sections for every setting. Runs on the game thread, like DoAutoSort.

enum class GridTarget : uintptr_t {
	PersonalStash = 1,
	Inventory     = 2,
	Cube          = 3,
};

struct GridTargetInfo {
	const char*          section;   // config section name
	Items::ItemContainer container;
};

GridTargetInfo InfoFor(GridTarget target) {
	switch (target) {
		case GridTarget::PersonalStash: return { "personal_stash", Items::ItemContainer::PersonalStash };
		case GridTarget::Inventory: return { "inventory", Items::ItemContainer::Inventory };
		case GridTarget::Cube: return { "cube", Items::ItemContainer::Cube };
	}
	return { "personal_stash", Items::ItemContainer::PersonalStash };
}

const ItemSorting::Grid::ParsedSection& SectionFor(const Config& cfg, GridTarget target) {
	switch (target) {
		case GridTarget::Inventory: return cfg.inventory;
		case GridTarget::Cube: return cfg.cube;
		case GridTarget::PersonalStash: break;
	}
	return cfg.personalStash;
}

// Applies a Grid::SequenceForLiveApply sequence: ONE move per transaction,
// in order, stopping at the first rejection. Every step lands on cells no
// other item occupies at that moment (that is what the sequence
// guarantees), so each is an ordinary single-item move.
//
// History: this used to send the whole plan as a single transaction, per the
// SDK README ("moved items are removed from a temporary occupancy model
// first, so a single transaction can swap or chain their locations"). A live
// run committed every move, yet the items moved onto cells another item of
// the same plan still occupied vanished from the grid once that item moved
// away (the cube and both tomes in the inventory, most of the personal
// stash). One transaction per safe step avoids depending on how a
// multi-operation transaction is applied. The price: a rejection midway
// leaves the grid partly sorted (nothing is lost -- every item is on a cell
// of its own).
auto ApplyGridMoves(
	const PluginContext* ctx, PlayerHandle player, Items::ItemContainer container, const std::vector<ItemSorting::PlannedMove>& steps,
	const std::vector<ItemSorting::ItemFacts>& items, uint32_t* committed) -> Items::Result {
	*committed = 0;
	for (size_t i = 0; i < steps.size(); ++i) {
		const auto& mv = steps[i];

		Items::ExistingItemOperation op {};
		op.structSize                       = Items::ExistingItemOperationSize;
		op.kind                             = Items::ExistingItemOperationKind::Move;
		op.item                             = static_cast<ItemHandle>(mv.handle);
		op.move.destination.structSize      = Items::ItemDestinationSize;
		op.move.destination.container       = container;
		op.move.destination.placement       = Items::Placement::Exact;
		op.move.destination.x               = static_cast<uint32_t>(mv.destX);
		op.move.destination.y               = static_cast<uint32_t>(mv.destY);
		op.move.destination.sharedStashPage = 0; // ignored outside SharedStash

		Items::ExistingItemTransaction txn {};
		txn.structSize     = Items::ExistingItemTransactionSize;
		txn.player         = player;
		txn.operationCount = 1;
		txn.operations     = &op;

		Items::ExistingItemTransactionResult result {};
		result.structSize = Items::ExistingItemTransactionResultSize;

		const auto txnResult = g_items->executeExistingItemTransaction(ctx, &txn, &result);
		if (txnResult == Items::Result::Success) {
			++*committed;
			continue;
		}

		std::string code = "?";
		std::string size = "?";
		for (const auto& it : items) {
			if (it.handle == mv.handle) {
				code = UnpackItemCode(it.code);
				size = std::to_string(it.w) + "x" + std::to_string(it.h);
				break;
			}
		}
		char msg[400];
		std::snprintf(msg, sizeof(msg),
			"%sthe game rejected step %zu/%zu (result %u): item '%s' (%s) -> (%d,%d). Stopped here; the %zu "
			"earlier step(s) stay applied, nothing is lost.",
			LogTagFor(container), i + 1, steps.size(), static_cast<unsigned>(txnResult), code.c_str(), size.c_str(), mv.destX, mv.destY, i);
		ctx->LogWarn(msg);
		return txnResult;
	}
	return Items::Result::Success;
}

// Re-reads the container after a sort and compares it with the scan before
// it: same number of items, every handle still present. A mismatch is logged
// loudly -- that is the symptom the first live run showed.
void VerifyGridAfterSort(const PluginContext* ctx, PlayerHandle player, Items::ItemContainer container,
	const std::vector<ItemSorting::ItemFacts>& before, const std::string& tag) {
	EnumState after;
	after.container = container;
	const Inventory::ItemFilter filter {
		.structSize    = Inventory::ItemFilterSize,
		.containerMask = Items::ContainerBit(container),
	};
	if (g_inventory->forEachInventoryItem(ctx, player, &filter, CollectSharedStashItem, &after) != Inventory::Result::Success) {
		ctx->LogWarn((tag + "post-sort check: could not re-read the container.").c_str());
		return;
	}
	std::unordered_map<uint64_t, bool> seen;
	for (const auto& it : after.facts) seen[it.handle] = true;
	size_t missing = 0;
	for (const auto& it : before) {
		if (!seen.count(it.handle)) ++missing;
	}
	// Overlaps in the re-read positions would mean two items claim one cell.
	size_t overlaps = 0;
	for (size_t i = 0; i < after.facts.size(); ++i)
		for (size_t j = i + 1; j < after.facts.size(); ++j) {
			const auto& a = after.facts[i];
			const auto& b = after.facts[j];
			if (a.x < b.x + static_cast<int32_t>(b.w) && b.x < a.x + static_cast<int32_t>(a.w) && a.y < b.y + static_cast<int32_t>(b.h)
				&& b.y < a.y + static_cast<int32_t>(a.h))
				++overlaps;
		}
	char buf[300];
	std::snprintf(buf, sizeof(buf), "%spost-sort check: %zu item(s) before, %zu after, %zu handle(s) missing, %zu overlapping pair(s).", tag.c_str(),
		before.size(), after.facts.size(), missing, overlaps);
	if (missing != 0 || overlaps != 0 || before.size() != after.facts.size()) {
		ctx->LogError((std::string(buf) + " SOMETHING WENT WRONG -- please send this log.").c_str());
	} else {
		ctx->LogInfo(buf);
	}
}

void __cdecl DoSortGrid(const PluginContext* ctx, void* userData) noexcept {
	namespace UiLayouts = ItemSorting::UiLayouts;
	namespace Grid      = ItemSorting::Grid;

	const auto target = static_cast<GridTarget>(reinterpret_cast<uintptr_t>(userData));
	const auto info   = InfoFor(target);
	const std::string tag = LogTagFor(info.container);

	// Hot reload: every setting of every section, `enabled` included. Also
	// refreshes mod_data_path's classification tables (see LoadConfig).
	if (target == GridTarget::PersonalStash) {
		const int64_t delta = g_lastPersonalSortClickMs.load() - g_lastAutoSortClickMs.load();
		if (delta > -kSameClickWindowMs && delta < kSameClickWindowMs) {
			ctx->LogInfo((tag + "ignored -- the same click also hit the shared stash's Auto Sort button on top of it (Shared tab).").c_str());
			return;
		}
	}

	const Config fresh = LoadConfig(ctx);
	if (!fresh.fileError.empty()) {
		ctx->LogWarn((tag + "sort skipped -- quyen19492-item-sorting.toml is not valid TOML: " + fresh.fileError).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortConfigError);
		return;
	}
	const Grid::ParsedSection& section = SectionFor(fresh, target);
	for (const auto& w : section.warnings) {
		ctx->LogWarn((tag + w).c_str());
	}
	if (!section.enabled) {
		ctx->LogInfo((tag + "sort skipped -- enabled = false.").c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortDisabled);
		return;
	}
	if (!section.errors.empty()) {
		for (const auto& e : section.errors) {
			ctx->LogWarn((tag + "config error: " + e).c_str());
		}
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortConfigError);
		return;
	}

	PlayerHandle player = InvalidPlayerHandle;
	if (g_inventory->getLocalPlayer(ctx, &player) != Inventory::Result::Success) {
		ctx->LogWarn((tag + "sort needs an authoritative local player.").c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	EnumState state;
	state.container = info.container;
	const Inventory::ItemFilter filter {
		.structSize    = Inventory::ItemFilterSize,
		.containerMask = Items::ContainerBit(info.container),
	};
	if (g_inventory->forEachInventoryItem(ctx, player, &filter, CollectSharedStashItem, &state) != Inventory::Result::Success) {
		ctx->LogWarn((tag + "could not enumerate the container.").c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	{
		std::string pages;
		for (const auto& [page, count] : state.itemsByInventoryPage) {
			if (!pages.empty()) pages += ", ";
			pages += "page " + std::to_string(page) + ": " + std::to_string(count);
		}
		char buf[512];
		std::snprintf(buf, sizeof(buf),
			"%sscanned %zu item(s) [%s]; %zu not in the gear/charm/jewel/ring/amulet table. Grid %ux%u, fill %s, break %s, overflow %s.",
			tag.c_str(), state.facts.size(), pages.c_str(), state.unclassifiedCount, section.config.width, section.config.height,
			Grid::ToString(section.config.fill), Grid::ToString(section.config.categoryBreak), Grid::ToString(section.config.onOverflow));
		ctx->LogInfo(buf);
	}

	// Safety net, unverified live: every item of one container is expected to
	// report the same ItemInfo::inventoryPage. If a mod's extra stash pages
	// (Reimagined's Gems / Materials / Runes tabs) turn out to be reported as
	// PersonalStash items on another page, their x/y are not cells of this
	// grid -- moving them with an Exact destination could pull them out of
	// those tabs. Refuse instead of guessing; the scan line above shows the
	// page numbers to fix this with.
	if (state.itemsByInventoryPage.size() > 1) {
		ctx->LogWarn((tag + "sort refused: the container reported items on more than one inventory page (see the scan line above). "
		                    "Nothing was moved -- please report this log.")
				.c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	const Grid::Plan plan = Grid::PlanSort(state.facts, section.config);
	for (const auto& line : plan.log) {
		ctx->LogInfo((tag + "plan: " + WithoutSectionTag(line, info.section)).c_str());
	}
	if (plan.status == Grid::Status::ConfigError) {
		ctx->LogWarn((tag + "config error: " + WithoutSectionTag(plan.error, info.section)).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortConfigError);
		return;
	}
	if (plan.status == Grid::Status::NoRoom) {
		ctx->LogWarn((tag + "sort failed, nothing moved: " + WithoutSectionTag(plan.error, info.section)).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortNoRoom);
		return;
	}

	// Never let the game see a move onto a cell another item still occupies
	// (see ApplyGridMoves): turn the plan into single-item steps that each
	// land on free cells, parking blockers aside where needed. Simulated in
	// full before anything moves.
	const auto sequence = Grid::SequenceForLiveApply(state.facts, plan.moves, section.config.width, section.config.height);
	if (!sequence.ok) {
		ctx->LogWarn((tag + "sort skipped, nothing moved: " + WithoutSectionTag(sequence.error, info.section)).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortTooFull);
		return;
	}
	{
		char buf[200];
		std::snprintf(buf, sizeof(buf), "%s%zu planned move(s) -> %zu live step(s) (%u temporary).", tag.c_str(), plan.moves.size(),
			sequence.steps.size(), sequence.parkingMoves);
		ctx->LogInfo(buf);
	}

	uint32_t   committed = 0;
	const auto txnResult = ApplyGridMoves(ctx, player, info.container, sequence.steps, state.facts, &committed);
	if (committed != 0) {
		VerifyGridAfterSort(ctx, player, info.container, state.facts, tag);
	}

	char summary[320];
	std::snprintf(summary, sizeof(summary),
		"%sarranged %u item(s) (%u fixed in place, %u outside the grid%s); %zu step(s), %u committed (result %u).", tag.c_str(),
		plan.itemsArranged, plan.itemsFixed, plan.itemsOutsideGrid, plan.compactRetryUsed ? ", needed the compact retry" : "",
		sequence.steps.size(), committed, static_cast<unsigned>(txnResult));
	ctx->LogInfo(summary);

	if (txnResult == Items::Result::NotAuthoritative) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::MultiplayerClientUnsupported);
		return;
	}
	if (txnResult != Items::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortRejected);
		return;
	}
	QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortOk);
}

// --- On-screen shared-stash page tracking --------------------------------
// See the g_selectedTabKnown comment near the top of this file.

void SetTrackedPage(const PluginContext* ctx, uint32_t page, const char* reason) {
	const bool     wasKnown = g_selectedTabKnown.load();
	const uint32_t previous = g_selectedTab.load();
	g_selectedTab.store(page);
	g_selectedTabKnown.store(true);
	if (ctx != nullptr && (!wasKnown || previous != page)) {
		char buf[200];
		std::snprintf(buf, sizeof(buf), "[remote-stash] on-screen shared page is now %u (%s).", page + 1, reason);
		ctx->LogInfo(buf);
	}
}

void MarkPageUnknown(const PluginContext* ctx, const char* reason) {
	if (!g_selectedTabKnown.exchange(false)) {
		return;
	}
	if (ctx != nullptr) {
		const std::string msg = std::string("[remote-stash] on-screen shared page is unknown until an item on it is hovered (") + reason + ").";
		ctx->LogInfo(msg.c_str());
	}
}

// Called for every UiMessage (text may be null for these native messages).
void TrackStashNavigation(const PluginContext* ctx, const char* target, const char* command, const char* text) {
	if (std::strcmp(target, "BankPanelMessage") == 0) {
		const bool right = std::strcmp(command, "SharedStashRight") == 0;
		const bool left  = std::strcmp(command, "SharedStashLeft") == 0;
		if ((!right && !left) || !g_selectedTabKnown.load()) {
			return;
		}
		const uint32_t current = g_selectedTab.load();
		const uint32_t maxTab  = g_config.tabCount == 0 ? 0 : g_config.tabCount - 1;
		// At either end D2R may clamp or wrap -- not verified, so don't guess.
		if (right && current >= maxTab) {
			MarkPageUnknown(ctx, "right arrow on the last page");
		} else if (left && current == 0) {
			MarkPageUnknown(ctx, "left arrow on the first page");
		} else {
			SetTrackedPage(ctx, right ? current + 1 : current - 1, right ? "page arrow right" : "page arrow left");
		}
		return;
	}
	if (std::strcmp(target, "PanelManager") == 0 && std::strcmp(command, "OpenPanel") == 0 && text != nullptr
		&& std::strcmp(text, "BankExpansionLayout") == 0) {
		MarkPageUnknown(ctx, "stash opened");
	}
}

// Contributes no tooltip text -- only used to learn which shared-stash page
// is on screen from the item under the cursor. Runs on the UI thread.
void __cdecl OnItemTooltip(const PluginContext* ctx, SharedEvents::ItemTooltipEvent* event, void*) noexcept {
	if (ctx == nullptr || event == nullptr || event->structSize < SharedEvents::ItemTooltipEventRequiredSize || g_items == nullptr) {
		return;
	}
	Items::ItemInfo info {};
	info.structSize = Items::ItemInfoSize;
	if (g_items->getItemInfo(ctx, event->item, &info) != Items::Result::Success) {
		return;
	}
	if (info.container != Items::ItemContainer::SharedStash || info.sharedStashPage == UINT32_MAX) {
		return;
	}
	SetTrackedPage(ctx, info.sharedStashPage, "hovered a shared-stash item");
}

// --- Add Page / Remove Page button visibility ------------------------------
// See g_addPageEnabled near the top of this file. The two buttons are widgets
// the layout patch (tools/patch_mod_stash_layout.py) inserted into the mod's
// BankExpansionLayout:
//   BankExpansionLayout/basicstash_container/SharedStashTabContainer/
//     Quyen19492ItemSortingStashButtons_v4/{QisAddBlankPageButton,QisRemovePageButton}
// Hiding one leaves an empty slot in the strip (the SDK can't move widgets).

constexpr const char* kAutoSortButtonWidget   = "QisAutoSortButton";
constexpr const char* kAddPageButtonWidget    = "QisAddBlankPageButton";
constexpr const char* kRemovePageButtonWidget = "QisRemovePageButton";

// findWidget should search every descendant of the panel; if this D2RLoader
// build only searches direct children, walk the known container chain.
bool FindStashButton(const PluginContext* ctx, Widgets::WidgetHandle panel, const char* name, Widgets::WidgetHandle* out) {
	if (g_widgets->findWidget(ctx, panel, name, out) == Widgets::Result::Success) {
		return true;
	}
	const char* const chain[] = { "basicstash_container", "SharedStashTabContainer", "Quyen19492ItemSortingStashButtons_v4" };
	Widgets::WidgetHandle current = panel;
	for (const char* step : chain) {
		Widgets::WidgetHandle next = Widgets::InvalidHandle;
		if (g_widgets->findWidget(ctx, current, step, &next) != Widgets::Result::Success) {
			return false;
		}
		current = next;
	}
	return g_widgets->findWidget(ctx, current, name, out) == Widgets::Result::Success;
}

// Re-reads only add_page_enabled / remove_page_enabled (not the whole
// LoadConfig, which also reloads the classification tables). UI thread.
void RefreshPageButtonToggles(const PluginContext* ctx) {
	std::string text;
	if (!ReadConfigText(ctx, &text)) {
		return;
	}
	toml::table parsed;
	try {
		parsed = toml::parse(text);
	} catch (const toml::parse_error&) {
		return; // keep the previous values; LoadConfig/Auto Sort log parse errors
	}
	auto shared = parsed["item-sorting"]["shared_stash"];
	g_sharedStashEnabled.store(shared["enabled"].value_or(true));
	g_addPageEnabled.store(shared["add_page_enabled"].value_or(true));
	g_removePageEnabled.store(shared["remove_page_enabled"].value_or(true));
}

void ApplyPageButtonVisibility(const PluginContext* ctx) {
	if (ctx == nullptr || g_widgets == nullptr) {
		return;
	}
	Widgets::WidgetHandle panel = Widgets::InvalidHandle;
	if (g_widgets->findPanel(ctx, "BankExpansionLayout", &panel) != Widgets::Result::Success) {
		return; // stash not loaded yet; the next OpenPanel retries
	}

	struct Button {
		const char* widget;
		bool        visible;
	};
	const bool   shared    = g_sharedStashEnabled.load();
	const Button buttons[] = {
		{ kAutoSortButtonWidget, shared },
		{ kAddPageButtonWidget, shared && g_addPageEnabled.load() },
		{ kRemovePageButtonWidget, shared && g_removePageEnabled.load() },
	};
	static std::atomic<bool> s_loggedNotFound { false };
	for (const Button& button : buttons) {
		Widgets::WidgetHandle w = Widgets::InvalidHandle;
		if (!FindStashButton(ctx, panel, button.widget, &w)) {
			// Only worth a warning when the user actually wants it hidden.
			if (!button.visible && !s_loggedNotFound.exchange(true)) {
				const std::string msg = std::string("[remote-stash] could not find the stash button '") + button.widget
				                      + "' to hide it (is the mod layout patched? tools/patch_mod_stash_layout.py). The action is still disabled.";
				ctx->LogWarn(msg.c_str());
			}
			continue;
		}
		const auto result = g_widgets->setWidgetVisible(ctx, w, button.visible);
		if (result != Widgets::Result::Success && !button.visible) {
			char buf[200];
			std::snprintf(buf, sizeof(buf), "[remote-stash] hiding stash button '%s' failed (result %u).", button.widget,
			               static_cast<unsigned>(result));
			ctx->LogWarn(buf);
		}
	}
}

void __cdecl ApplyPageButtonVisibilityOnUiThread(const PluginContext* ctx, void*) noexcept {
	ApplyPageButtonVisibility(ctx);
}

// Called for every UiMessage. The listener sees a message before the panel
// handles it, so the widget work is queued to run after this dispatch (the
// panel may only be created by the OpenPanel being dispatched right now).
void TrackPageButtonVisibility(const PluginContext* ctx, const char* target, const char* command, const char* text) {
	const bool stashOpened = std::strcmp(target, "PanelManager") == 0 && std::strcmp(command, "OpenPanel") == 0 && text != nullptr
	                      && std::strcmp(text, "BankExpansionLayout") == 0;
	const bool stashTabSwitched = std::strcmp(target, "BankPanelMessage") == 0 && std::strcmp(command, "SelectTab") == 0;
	if (!stashOpened && !stashTabSwitched) {
		return;
	}
	if (stashOpened) {
		RefreshPageButtonToggles(ctx);
	}
	(void)g_threads->runOnUiThread(ctx, ApplyPageButtonVisibilityOnUiThread, nullptr);
}

// --- UI message listener (button clicks) --------------------------------

auto __cdecl OnUiMessage(const PluginContext* ctx, const SharedEvents::UiMessageEvent* event, void*) noexcept -> SharedEvents::UiMessageAction {
	namespace UiLayouts = ItemSorting::UiLayouts;

	if (ctx == nullptr || event == nullptr || event->structSize < SharedEvents::UiMessageEventRequiredSize) {
		return SharedEvents::UiMessageAction::Continue;
	}

	// Debug aid for "Known gaps" #6/#7: with debug_log_ui_messages = true in
	// the config, log every message this listener sees -- ours, every other
	// plugin's, and D2R's own native panel/button messages -- so the exact
	// target/command D2R fires when Shared Stash opens, closes, or switches
	// tabs can be found by watching the log while using the stash in-game.
	// Off by default; this fires on essentially every UI click in the game.
	if (g_config.debugLogUiMessages) {
		char buf[256];
		std::snprintf(buf, sizeof(buf), "[plugin] debug: UiMessage target=\"%s\" command=\"%s\" text=\"%s\"",
		               event->target != nullptr ? event->target : "(null)",
		               event->command != nullptr ? event->command : "(null)",
		               event->text != nullptr ? event->text : "(null)");
		ctx->LogInfo(buf);
	}

	if (event->target != nullptr && event->command != nullptr) {
		TrackStashNavigation(ctx, event->target, event->command, event->text);
		TrackPageButtonVisibility(ctx, event->target, event->command, event->text);
	}

	if (event->target == nullptr || event->command == nullptr || event->text == nullptr) {
		return SharedEvents::UiMessageAction::Continue;
	}

	// Only D2R-native "PanelManager:TogglePanel" messages against one of our
	// three dedicated trigger panels are ours to handle -- see the trigger-
	// panel comment on the g_action*Panel globals above for why buttons are
	// wired this way instead of a plugin-invented onClickMessage target.
	if (std::strcmp(event->target, "PanelManager") != 0 || std::strcmp(event->command, "TogglePanel") != 0) {
		return SharedEvents::UiMessageAction::Continue;
	}

	struct ActionBinding {
		const char*                logicalName;
		Panels::RegistrationHandle handle;
	};
	const ActionBinding bindings[] = {
		{ "quyen19492-item-sorting/ActionAutoSort", g_actionAutoSortPanel },
		{ "quyen19492-item-sorting/ActionAddBlankPage", g_actionAddBlankPagePanel },
		{ "quyen19492-item-sorting/ActionRemoveCurrentPage", g_actionRemoveCurrentPagePanel },
		{ "quyen19492-item-sorting/ActionSortPersonalStash", g_actionSortPersonalStashPanel },
		{ "quyen19492-item-sorting/ActionSortInventory", g_actionSortInventoryPanel },
		{ "quyen19492-item-sorting/ActionSortCube", g_actionSortCubePanel },
	};
	const ActionBinding* matched = nullptr;
	for (const ActionBinding& binding : bindings) {
		if (std::strcmp(event->text, binding.logicalName) == 0) {
			matched = &binding;
			break;
		}
	}
	if (matched == nullptr) {
		// Some other plugin's or D2R's own panel toggled -- not ours.
		return SharedEvents::UiMessageAction::Continue;
	}

	// TogglePanel just opened this (always-closed, invisible) trigger panel;
	// close it right back so the next click toggles it open again instead of
	// closed. Defensive/idempotent either way -- see the comment above.
	if (matched->handle != Panels::InvalidHandle) {
		(void)g_panels->closePanel(ctx, matched->handle);
	}

	// Single-grid Sort buttons. `enabled` and every other setting are checked
	// on the game thread after a fresh config read (hot reload), not here.
	GridTarget gridTarget {};
	bool       isGridAction = true;
	if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionAutoSort") == 0) {
		g_lastAutoSortClickMs.store(NowMs()); // see g_lastAutoSortClickMs
	}
	if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionSortPersonalStash") == 0) {
		g_lastPersonalSortClickMs.store(NowMs());
		gridTarget = GridTarget::PersonalStash;
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionSortInventory") == 0) {
		gridTarget = GridTarget::Inventory;
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionSortCube") == 0) {
		gridTarget = GridTarget::Cube;
	} else {
		isGridAction = false;
	}
	if (isGridAction) {
		void* arg = reinterpret_cast<void*>(static_cast<uintptr_t>(gridTarget));
		if (g_threads->runOnGameThread(ctx, DoSortGrid, arg) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
		return SharedEvents::UiMessageAction::Consume;
	}

	// Shared-stash buttons. [shared_stash] enabled = false normally hides
	// them (ApplyPageButtonVisibility); this catches a click anyway. The
	// game-thread action re-checks it from a fresh config read.
	if (!g_sharedStashEnabled.load()) {
		ctx->LogInfo("[remote-stash] shared stash is disabled ([item-sorting.shared_stash] enabled = false); ignoring the click.");
		ShowToast(ctx, UiLayouts::OutcomeMessage::AutoSortDisabled);
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionAutoSort") == 0) {
		if (g_threads->runOnGameThread(ctx, DoAutoSort, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionAddBlankPage") == 0 && !g_addPageEnabled.load()) {
		ctx->LogInfo("[remote-stash] Add Page is disabled (add_page_enabled = false); ignoring the click.");
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionRemoveCurrentPage") == 0 && !g_removePageEnabled.load()) {
		ctx->LogInfo("[remote-stash] Remove Page is disabled (remove_page_enabled = false); ignoring the click.");
	} else if ((std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionAddBlankPage") == 0
				   || std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionRemoveCurrentPage") == 0)
			   && !g_selectedTabKnown.load()) {
		ctx->LogInfo("[remote-stash] page action skipped -- the on-screen shared page is unknown (hover an item on it first).");
		ShowToast(ctx, UiLayouts::OutcomeMessage::PageUnknown);
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionAddBlankPage") == 0) {
		if (g_threads->runOnGameThread(ctx, DoAddBlankPage, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if (std::strcmp(matched->logicalName, "quyen19492-item-sorting/ActionRemoveCurrentPage") == 0) {
		if (g_threads->runOnGameThread(ctx, DoRemoveCurrentPage, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	}

	return SharedEvents::UiMessageAction::Consume;
}

} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const PluginInfo* {
	return &kPluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}

	if (context->QueryService(&g_inventory) != ServiceQueryResult::Success || !HasInventoryServiceField(g_inventory, InventoryServiceRequiredSize)) {
		context->LogError("[plugin] InventoryService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_items) != ServiceQueryResult::Success || !HasItemServiceField(g_items, ItemServiceRequiredSize)) {
		context->LogError("[plugin] ItemService is unavailable.");
		return false;
	}
	if (!HasItemServiceCapability(g_items, Items::ItemServiceCapability::SharedStashWrite)) {
		context->LogError("[plugin] this D2RLoader build does not support shared-stash item writes (needs ItemServiceCapability::SharedStashWrite).");
		return false;
	}
	if (context->QueryService(&g_threads) != ServiceQueryResult::Success || !HasThreadServiceField(g_threads, ThreadServiceRequiredSize)) {
		context->LogError("[plugin] ThreadService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_panels) != ServiceQueryResult::Success || !HasPanelServiceField(g_panels, PanelServiceRequiredSize)) {
		context->LogError("[plugin] PanelService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_widgets) != ServiceQueryResult::Success || !HasWidgetServiceField(g_widgets, WidgetServiceRequiredSize)) {
		context->LogError("[plugin] WidgetService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_sharedEvents) != ServiceQueryResult::Success || !HasSharedEventServiceField(g_sharedEvents, SharedEventServiceRequiredSize)) {
		context->LogError("[plugin] SharedEventService is unavailable.");
		return false;
	}

	g_config = LoadConfig(context);
	if (!g_config.enabled) {
		// Plugin-wide master switch ([item-sorting] enabled), read
		// once here. Register nothing: no toast, no actions, no inventory
		// button. Buttons already patched into the mod's layouts stay
		// visible but their clicks go nowhere.
		context->LogInfo("[plugin] disabled ([item-sorting] enabled = false); nothing registered.");
		return true;
	}
	g_sharedStashEnabled.store(g_config.sharedEnabled);
	g_addPageEnabled.store(g_config.addPageEnabled);
	g_removePageEnabled.store(g_config.removePageEnabled);

	// Toast panel. Its layout is data/global/ui/layouts/quyen19492-item-sorting/
	// Toasthd.json in the companion mpq -- see ui_layouts.hpp.
	const Panels::PanelRegistration toastRegistration {
		.structSize = Panels::PanelRegistrationSize,
		.flags      = Panels::PanelFlags::CloseOnEscape,
		.localId    = "Toast",
	};
	if (g_panels->registerPanel(context, &toastRegistration, &g_toastPanel) != Panels::Result::Success) {
		context->LogError("[plugin] could not register the toast panel.");
		return false;
	}

	// Invisible trigger panels, one per button -- see the g_action*Panel
	// declarations near the top of this file for why. Each layout file is
	// data/global/ui/layouts/quyen19492-item-sorting/Action<Name>hd.json in the
	// companion mpq, a 1x1 Panel with no children.
	struct ActionPanelSetup {
		const char*                 localId;
		Panels::RegistrationHandle* handle;
	};
	const ActionPanelSetup actionPanels[] = {
		{ "ActionAutoSort", &g_actionAutoSortPanel },
		{ "ActionAddBlankPage", &g_actionAddBlankPagePanel },
		{ "ActionRemoveCurrentPage", &g_actionRemoveCurrentPagePanel },
		{ "ActionSortPersonalStash", &g_actionSortPersonalStashPanel },
		{ "ActionSortInventory", &g_actionSortInventoryPanel },
		{ "ActionSortCube", &g_actionSortCubePanel },
	};
	for (const ActionPanelSetup& setup : actionPanels) {
		const Panels::PanelRegistration registration {
			.structSize = Panels::PanelRegistrationSize,
			.localId    = setup.localId,
		};
		if (g_panels->registerPanel(context, &registration, setup.handle) != Panels::Result::Success) {
			context->LogError("[plugin] could not register a button trigger panel.");
			return false;
		}
	}

	// The inventory's Sort button: a plugin child layout on the stock
	// PlayerInventory panel. Its file is data/global/ui/layouts/
	// quyen19492-item-sorting/InventoryToolshd.json in the companion mpq (a
	// single ButtonWidget root -- see that file). Keyboard/mouse only: its
	// position is in mouse-layout coordinates. Not fatal if it fails: every
	// other button still works.
	const Panels::ChildLayoutRegistration inventoryTools {
		.structSize = Panels::ChildLayoutRegistrationSize,
		.flags      = Panels::ChildLayoutFlags::KeyboardMouseOnly,
		.stockPanel = Panels::StockPanel::PlayerInventory,
		.localId    = "InventoryTools",
	};
	if (const auto r = g_panels->registerChildLayout(context, &inventoryTools, &g_inventoryToolsChildLayout); r != Panels::Result::Success) {
		char buf[200];
		std::snprintf(buf, sizeof(buf), "[inventory] could not add the Sort button to the inventory panel (result %u); inventory sorting has no button.",
		               static_cast<unsigned>(r));
		context->LogWarn(buf);
	} else {
		context->LogInfo("[inventory] Sort button registered (child layout InventoryTools on PlayerInventory).");
	}

	const SharedEvents::UiMessageListener listener {
		.structSize = SharedEvents::UiMessageListenerSize,
		.callback   = OnUiMessage,
	};
	if (g_sharedEvents->registerUiMessageListener(context, &listener, &g_uiMessageListener) != SharedEvents::Result::Success) {
		context->LogError("[plugin] could not register the UI message listener.");
		return false;
	}

	const SharedEvents::ItemTooltipListener tooltipListener {
		.structSize = SharedEvents::ItemTooltipListenerSize,
		.region     = SharedEvents::ItemTooltipRegion::Description,
		.position   = SharedEvents::ItemTooltipPosition::Bottom,
		.fallback   = SharedEvents::ItemTooltipFallback::Omit,
		.callback   = OnItemTooltip,
	};
	if (g_sharedEvents->registerItemTooltipListener(context, &tooltipListener, &g_tooltipListener) != SharedEvents::Result::Success) {
		context->LogWarn("[plugin] could not register the item tooltip listener; the on-screen page can only be set with the page arrows / plugin buttons.");
	}

	context->LogInfo("[plugin] loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	// D2RLoader unregisters every resource/panel/child-layout/listener this
	// plugin owns automatically on unload (see PluginSDK README "Panels,
	// Layouts, and Widgets" / "Shared UI Events"), so there is nothing to do
	// here explicitly -- matches every SDK example.
}
