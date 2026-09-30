// Quyen194 Item Sorting -- D2RLoader plugin entry point.
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
	.id          = Quyen194ItemSorting::UiLayouts::kPluginId,
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
const ResourceService*    g_resources    = nullptr;
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
SharedEvents::ListenerHandle g_tooltipListener = SharedEvents::InvalidHandle;

struct Config {
	bool                                        enabled    = true;
	uint32_t                                    gridWidth  = 16;
	uint32_t                                    gridHeight = 13;
	uint32_t                                    tabCount   = 20;
	std::vector<Quyen194ItemSorting::CategorySpec> layout;
	// Debug aid for the "Known gaps" #6/#7 live-layout work: when true, every
	// incoming UiMessageEvent (ours and every other plugin's/D2R's own) is
	// logged as target/command, so the exact native message D2R fires when
	// the Shared Stash panel opens/closes/switches tabs can be found by
	// watching the log while toggling it in-game. Off by default -- this is
	// a firehose, not something to leave on.
	bool                                        debugLogUiMessages = false;

	// Mirrors the save-editor's autosort.json layer (startTab,
	// blankTabsAtStart, compactEmptyTabsFirst, addEmptyPageWhen) -- see
	// sort_engine.hpp's AutoSortConfig comment for exactly what is and isn't
	// ported (the quota-shortfall "padding" pass is not -- it doesn't apply
	// to how this engine reserves tabs).
	uint32_t                                    startTab              = 0;
	uint32_t                                    blankTabsAtStart      = 3;
	bool                                        compactEmptyTabsFirst = true;
	Quyen194ItemSorting::EmptyPageRule           addEmptyPageWhen      = Quyen194ItemSorting::EmptyPageRule::AtPercent(30.0);

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
	Quyen194ItemSorting::Grid::ParsedSection personalStash;
	Quyen194ItemSorting::Grid::ParsedSection inventory;
	Quyen194ItemSorting::Grid::ParsedSection cube;
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
bool TryParseEmptyPageRule(toml::node_view<Node> node, Quyen194ItemSorting::EmptyPageRule* outRule) {
	if (!node) {
		return false;
	}
	if (const auto* s = node.as_string()) {
		if (s->get() == "never") {
			*outRule = Quyen194ItemSorting::EmptyPageRule::Never();
			return true;
		}
		if (s->get() == "always") {
			*outRule = Quyen194ItemSorting::EmptyPageRule::AtPercent(0.0);
			return true;
		}
		return false;
	}
	if (auto d = node.template value<double>()) {
		if (*d >= 0.0 && *d <= 100.0) {
			*outRule = Quyen194ItemSorting::EmptyPageRule::AtPercent(*d);
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

Config LoadConfig(const PluginContext* ctx) {
	Config cfg;
	cfg.layout        = Quyen194ItemSorting::DefaultLayout();
	cfg.personalStash.config = Quyen194ItemSorting::Grid::DefaultConfig("personal_stash");
	cfg.inventory.config     = Quyen194ItemSorting::Grid::DefaultConfig("inventory");
	cfg.cube.config          = Quyen194ItemSorting::Grid::DefaultConfig("cube");
	if (ctx == nullptr) {
		return cfg;
	}

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
		ctx->LogWarn("quyen194-item-sorting: could not read quyen194-item-sorting.toml; using built-in defaults.");
		return cfg;
	}
	if (required > buffer.size()) {
		ctx->LogWarn("quyen194-item-sorting: config file is larger than expected; using built-in defaults.");
		return cfg;
	}

	std::string text(buffer.data(), buffer.data() + required);
	while (!text.empty() && text.back() == '\0') {
		text.pop_back();
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
		const std::string msg = std::string("quyen194-item-sorting: config parse error: ") + std::string(err.description());
		ctx->LogWarn(msg.c_str());
		cfg.fileError = std::string(err.description()) + " (line " + std::to_string(err.source().begin.line) + ")";
		return cfg;
	}

	cfg.personalStash = Quyen194ItemSorting::Grid::ParseGridSection(parsed, "personal_stash");
	cfg.inventory     = Quyen194ItemSorting::Grid::ParseGridSection(parsed, "inventory");
	cfg.cube          = Quyen194ItemSorting::Grid::ParseGridSection(parsed, "cube");

	auto section = parsed["quyen194-item-sorting"];
	cfg.enabled            = section["enabled"].value_or(true);
	cfg.tabCount           = section["shared_stash_tab_count"].value_or(20u);
	cfg.gridWidth          = section["grid"]["width"].value_or(16u);
	cfg.gridHeight         = section["grid"]["height"].value_or(13u);
	cfg.debugLogUiMessages = section["debug_log_ui_messages"].value_or(false);

	cfg.startTab              = section["start_tab"].value_or(0u);
	cfg.blankTabsAtStart      = section["blank_tabs_at_start"].value_or(3u);
	cfg.compactEmptyTabsFirst = section["compact_empty_tabs_first"].value_or(true);
	{
		auto node = section["add_empty_page_when"];
		Quyen194ItemSorting::EmptyPageRule rule;
		if (TryParseEmptyPageRule(node, &rule)) {
			cfg.addEmptyPageWhen = rule;
		} else if (node) {
			ctx->LogWarn(
				"quyen194-item-sorting: add_empty_page_when must be \"never\", \"always\", or a number 0-100; using the built-in "
				"default (30).");
		}
	}

	if (auto layoutArr = section["layout"].as_array()) {
		std::vector<Quyen194ItemSorting::CategorySpec> parsedLayout;
		bool                                           allKnown = true;
		for (auto&& node : *layoutArr) {
			const auto* row = node.as_table();
			if (row == nullptr) {
				continue;
			}
			std::string modeId  = (*row)["mode_id"].value_or(std::string {});
			uint32_t    minTabs = (*row)["min_tabs"].value_or(0u);
			if (modeId.empty() || !Quyen194ItemSorting::IsKnownModeId(modeId)) {
				allKnown = false;
				break;
			}

			Quyen194ItemSorting::CategorySpec spec;
			spec.id         = modeId;
			spec.minTabs    = minTabs;
			spec.sortKeys   = ParseStringArray((*row)["sort_keys"]);
			spec.gearGroups = ParseStringArray((*row)["gear_groups"]);

			auto                               overrideNode = (*row)["add_empty_page_when"];
			Quyen194ItemSorting::EmptyPageRule catRule;
			if (TryParseEmptyPageRule(overrideNode, &catRule)) {
				spec.hasEmptyPageOverride = true;
				spec.emptyPageOverride    = catRule;
			} else if (overrideNode) {
				ctx->LogWarn(
					("quyen194-item-sorting: [[layout]] entry '" + modeId
					 + "' has an invalid add_empty_page_when; inheriting the global setting.")
						.c_str());
			}

			parsedLayout.push_back(std::move(spec));
		}
		if (allKnown && !parsedLayout.empty()) {
			cfg.layout = std::move(parsedLayout);
		} else if (!allKnown) {
			ctx->LogWarn("quyen194-item-sorting: config has an unknown mode_id in [[layout]]; using the built-in default layout instead.");
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
		Quyen194ItemSorting::Classification::UseEmbeddedTable();
	} else {
		std::string loadError;
		if (!Quyen194ItemSorting::Classification::LoadFromModFolder(cfg.modDataPath, &loadError)) {
			ctx->LogWarn(
				("quyen194-item-sorting: mod_data_path is set to '" + cfg.modDataPath + "' but could not be used (" + loadError
				 + "); falling back to the embedded classification table.")
					.c_str());
			// LoadFromModFolder already reverted to the embedded table on failure.
		}
	}

	return cfg;
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
	std::vector<Quyen194ItemSorting::ItemFacts> facts;

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

	Quyen194ItemSorting::ItemFacts fact;
	fact.handle          = item->handle;
	fact.code            = item->code;
	fact.quality         = static_cast<uint32_t>(item->quality);
	fact.itemLevel       = item->itemLevel;
	fact.sharedStashPage = item->sharedStashPage;
	fact.x               = item->x;
	fact.y               = item->y;

	uint32_t baseLevelReq = 0;
	if (auto cls = Quyen194ItemSorting::Classification::Classify(item->code)) {
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
	if (auto misc = Quyen194ItemSorting::Classification::ClassifyMisc(item->code)) {
		fact.miscKind  = misc->kind;
		fact.miscOrder = misc->order;
		fact.miscGrade = misc->grade;
		if (!Quyen194ItemSorting::Classification::Classify(item->code)) {
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
	fact.sortLevel = Quyen194ItemSorting::Classification::TrueReqLevel(fact.quality, item->qualityRecordId, item->prefixIds, item->suffixIds, baseLevelReq);

	if (item->quality == Items::Quality::Set) {
		fact.setId        = item->qualityRecordId;
		fact.setGroupName = Quyen194ItemSorting::Classification::SetGroupForRow(item->qualityRecordId);

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
				"quyen194-item-sorting: set item handle %llu (code='%s') has qualityRecordId=%d, which "
				"could not be mapped to a set name; Auto Sort will leave it exactly where it is (see "
				"README Known gaps #3).",
				static_cast<unsigned long long>(item->handle), UnpackItemCode(item->code).c_str(), item->qualityRecordId);
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
			"quyen194-item-sorting: shared-stash scan: %zu item(s) across %zu distinct tab number(s) [%s]. If this "
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
			"quyen194-item-sorting: item classification: %zu matched, %zu unmatched (defaults used -- excluded from "
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
	const PluginContext* ctx, PlayerHandle player, const Quyen194ItemSorting::SequencedMoves& sequenced,
	const std::vector<Quyen194ItemSorting::ItemFacts>& items, uint32_t* committedCount) -> Items::Result {
	if (committedCount != nullptr) {
		*committedCount = 0;
	}
	const auto& moves = sequenced.moves;
	if (moves.empty()) {
		return Items::Result::Success;
	}

	// Handle -> facts, purely for the rejection log below (what code/category
	// was this?) -- never consulted for planning, which already happened.
	std::unordered_map<uint64_t, const Quyen194ItemSorting::ItemFacts*> factsByHandle;
	factsByHandle.reserve(items.size());
	for (const auto& it : items) factsByHandle.emplace(it.handle, &it);

	for (const auto& note : sequenced.diagnostics) {
		const std::string msg = "quyen194-item-sorting: move ordering: " + note;
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
						case Quyen194ItemSorting::NonGearKind::Charm: category = "charm"; break;
						case Quyen194ItemSorting::NonGearKind::Jewel: category = "jewel"; break;
						case Quyen194ItemSorting::NonGearKind::Ring: category = "ring"; break;
						case Quyen194ItemSorting::NonGearKind::Amulet: category = "amulet"; break;
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
				"quyen194-item-sorting: move for item handle %llu (code='%s', %s) -> tab %u (%u,%u) was rejected (result "
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
				"quyen194-item-sorting: a single dependency layer exceeds 4096 operations; it will be sent in several "
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

void ShowToast(const PluginContext* ctx, Quyen194ItemSorting::UiLayouts::OutcomeMessage message) {
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;
	if (ctx == nullptr || g_panels == nullptr || g_widgets == nullptr) {
		return;
	}
	if (g_panels->openPanel(ctx, g_toastPanel) != Panels::Result::Success) {
		return;
	}

	Widgets::WidgetHandle panelWidget = Widgets::InvalidHandle;
	if (g_widgets->findPanel(ctx, "quyen194-item-sorting/Toast", &panelWidget) != Widgets::Result::Success) {
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
	Quyen194ItemSorting::UiLayouts::OutcomeMessage message;
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
void QueueToast(const PluginContext* ctx, Quyen194ItemSorting::UiLayouts::OutcomeMessage message) {
	auto* req = new (std::nothrow) ToastRequest { message };
	if (req == nullptr) {
		return;
	}
	if (g_threads->runOnUiThread(ctx, ShowToastOnUiThread, req) != Threads::Result::Success) {
		delete req;
	}
}

// --- Button actions (run on the game thread) --------------------------

void __cdecl DoAutoSort(const PluginContext* ctx, void*) noexcept {
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;

	// Hot-reload: re-read the config file fresh on every Auto Sort run and
	// take every setting except the three that only take effect at plugin
	// load time and therefore need a game restart to change (`enabled`,
	// `debug_log_ui_messages`, and the [grid] width/height) -- see the config
	// file's own "What reloads when" comment. This runs on the game thread
	// (same as the rest of DoAutoSort), so g_config is never read mid-update
	// by another thread.
	{
		Config fresh                   = LoadConfig(ctx);
		g_config.tabCount              = fresh.tabCount;
		g_config.layout                = std::move(fresh.layout);
		g_config.startTab              = fresh.startTab;
		g_config.blankTabsAtStart      = fresh.blankTabsAtStart;
		g_config.compactEmptyTabsFirst = fresh.compactEmptyTabsFirst;
		g_config.addEmptyPageWhen      = fresh.addEmptyPageWhen;
	}

	PlayerHandle player = InvalidPlayerHandle;
	if (g_inventory->getLocalPlayer(ctx, &player) != Inventory::Result::Success) {
		ctx->LogWarn("quyen194-item-sorting: auto sort needs an authoritative local player.");
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	EnumState state;
	if (!CollectSharedStash(ctx, player, &state)) {
		ctx->LogWarn("quyen194-item-sorting: could not enumerate the shared stash.");
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	Quyen194ItemSorting::AutoSortConfig sortCfg;
	sortCfg.layout                = g_config.layout;
	sortCfg.tabCount              = g_config.tabCount;
	sortCfg.gridWidth             = g_config.gridWidth;
	sortCfg.gridHeight            = g_config.gridHeight;
	sortCfg.startTab              = g_config.startTab;
	sortCfg.blankTabsAtStart      = g_config.blankTabsAtStart;
	sortCfg.compactEmptyTabsFirst = g_config.compactEmptyTabsFirst;
	sortCfg.addEmptyPageWhen      = g_config.addEmptyPageWhen;

	const auto outcome = Quyen194ItemSorting::RunAutoSortWithPadding(state.facts, sortCfg);
	if (!outcome.ok) {
		const std::string logMsg = "quyen194-item-sorting: auto sort failed: " + outcome.error;
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
		const std::string msg = "quyen194-item-sorting: plan: " + line;
		ctx->LogInfo(msg.c_str());
	}

	{
		char padMsg[256];
		std::snprintf(
			padMsg, sizeof(padMsg),
			"quyen194-item-sorting: compacted %u item(s) to relabel empty tabs to the end; added %u breathing page(s); pulled %u/%u "
			"blank tab(s) to the front.",
			outcome.compactMoveCount, outcome.breathingTabsAdded, outcome.blankTabsAtStartAdded, g_config.blankTabsAtStart);
		ctx->LogInfo(padMsg);
	}

	// Diagnostic: replay the plan against each item's live footprint and flag
	// any two moves that would land on the same cell(s). This tells apart a
	// planning-side bug (RunAutoSort / item classification data) from an
	// application-side one (ApplyMoves / the live transaction) when the
	// in-game result looks wrong -- see README "Known gaps".
	const auto collisions = Quyen194ItemSorting::DetectCollisions(state.facts, outcome.moves, g_config.gridWidth, g_config.gridHeight);
	if (!collisions.empty()) {
		char summary[192];
		std::snprintf(
			summary, sizeof(summary), "quyen194-item-sorting: plan self-check found %zu colliding move(s) (showing up to 20):",
			collisions.size());
		ctx->LogWarn(summary);
		for (const auto& line : collisions) {
			const std::string msg = "quyen194-item-sorting: " + line;
			ctx->LogWarn(msg.c_str());
		}
	} else {
		ctx->LogInfo("quyen194-item-sorting: plan self-check found no colliding moves.");
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
	const auto sequenced = Quyen194ItemSorting::SequenceMovesForLiveApply(
		state.facts, outcome.moves, g_config.tabCount, g_config.gridWidth, g_config.gridHeight, outcome.tabsUsed);

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, sequenced, state.facts, &committed);

	char logMsg[320] {};
	std::snprintf(
		logMsg,
		sizeof(logMsg),
		"quyen194-item-sorting: auto sort placed %u item(s) across %u tab(s); committed %u/%zu move op(s) (transaction result %u).",
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
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;

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

	const auto outcome = Quyen194ItemSorting::MoveBlankTabAfter(state.facts, g_selectedTab.load(), g_config.tabCount);
	if (!outcome.ok) {
		const std::string logMsg = "quyen194-item-sorting: add blank page after page " + std::to_string(g_selectedTab.load() + 1) + " failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::NoBlankTab);
		return;
	}

	// See DoAutoSort's own comment on SequenceMovesForLiveApply: a tab-shift
	// like this one moves a whole chain of tabs back one slot, and the same
	// "destination still occupied by another in-plan item" issue can apply
	// here too, just at tab granularity instead of per-cell.
	const auto sequenced =
		Quyen194ItemSorting::SequenceMovesForLiveApply(state.facts, outcome.moves, g_config.tabCount, g_config.gridWidth, g_config.gridHeight);

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
		std::snprintf(buf, sizeof(buf), "quyen194-item-sorting: add blank page after page %u: blank is now page %u; committed %u/%zu move(s).",
		               g_selectedTab.load() + 1, outcome.blankTab + 1, committed, sequenced.moves.size());
		ctx->LogInfo(buf);
	}
	QueueToast(ctx, UiLayouts::OutcomeMessage::BlankMoved);
}

void __cdecl DoRemoveCurrentPage(const PluginContext* ctx, void*) noexcept {
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;

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

	const auto outcome = Quyen194ItemSorting::PushBlankTabToEnd(state.facts, g_selectedTab.load(), g_config.tabCount);
	if (!outcome.ok) {
		const std::string logMsg = "quyen194-item-sorting: remove page " + std::to_string(g_selectedTab.load() + 1) + " failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());
		const auto msg = outcome.error.find("out of range") != std::string::npos ? UiLayouts::OutcomeMessage::OutOfRange : UiLayouts::OutcomeMessage::CurrentNotEmpty;
		QueueToast(ctx, msg);
		return;
	}

	// See DoAutoSort's own comment on SequenceMovesForLiveApply.
	const auto sequenced =
		Quyen194ItemSorting::SequenceMovesForLiveApply(state.facts, outcome.moves, g_config.tabCount, g_config.gridWidth, g_config.gridHeight);

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
		std::snprintf(buf, sizeof(buf), "quyen194-item-sorting: removed empty page %u (moved to the end); committed %u/%zu move(s).",
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
	const char*          section;   // config section name, also the log tag
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

const Quyen194ItemSorting::Grid::ParsedSection& SectionFor(const Config& cfg, GridTarget target) {
	switch (target) {
		case GridTarget::Inventory: return cfg.inventory;
		case GridTarget::Cube: return cfg.cube;
		case GridTarget::PersonalStash: break;
	}
	return cfg.personalStash;
}

// Sends the whole plan as ONE transaction. The SDK validates every operation
// against an occupancy model with all moved items lifted out first, so swaps
// and chains need no ordering, and on any failure it restores every item --
// which is exactly the "never a half-sorted grid" promise the config file
// makes. (The shared stash's layered/bisecting ApplyMoves exists because a
// full shared-stash plan can exceed one transaction's 4,096 operations; a
// single grid holds at most 16 x 16 = 256 items.)
auto ApplyGridMoves(
	const PluginContext* ctx, PlayerHandle player, Items::ItemContainer container, const std::vector<Quyen194ItemSorting::PlannedMove>& moves,
	const std::vector<Quyen194ItemSorting::ItemFacts>& items, const char* section, uint32_t* committed) -> Items::Result {
	*committed = 0;
	if (moves.empty()) {
		return Items::Result::Success;
	}
	if (moves.size() > Items::MaxExistingItemOperations) {
		ctx->LogWarn("quyen194-item-sorting: single-grid plan has more moves than one transaction allows; refusing to split it.");
		return Items::Result::InvalidArgument;
	}

	std::vector<Items::ExistingItemOperation> ops;
	ops.reserve(moves.size());
	for (const auto& mv : moves) {
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
		ops.push_back(op);
	}

	Items::ExistingItemTransaction txn {};
	txn.structSize     = Items::ExistingItemTransactionSize;
	txn.player         = player;
	txn.operationCount = static_cast<uint32_t>(ops.size());
	txn.operations     = ops.data();

	Items::ExistingItemTransactionResult result {};
	result.structSize = Items::ExistingItemTransactionResultSize;

	const auto txnResult = g_items->executeExistingItemTransaction(ctx, &txn, &result);
	if (txnResult == Items::Result::Success) {
		*committed = result.committedOperationCount;
		return txnResult;
	}

	char msg[400];
	if (result.failureIndex < ops.size()) {
		const auto& op   = ops[result.failureIndex];
		std::string code = "?";
		std::string size = "?";
		int32_t     fromX = -1, fromY = -1;
		for (const auto& it : items) {
			if (it.handle == static_cast<uint64_t>(op.item)) {
				code  = UnpackItemCode(it.code);
				size  = std::to_string(it.w) + "x" + std::to_string(it.h);
				fromX = it.x;
				fromY = it.y;
				break;
			}
		}
		std::snprintf(msg, sizeof(msg),
			"quyen194-item-sorting: [%s] the game rejected the sort (result %u) at move %u/%zu: item '%s' (%s) (%d,%d) -> (%u,%u). "
			"Nothing was moved.",
			section, static_cast<unsigned>(txnResult), result.failureIndex + 1, ops.size(), code.c_str(), size.c_str(), fromX, fromY,
			op.move.destination.x, op.move.destination.y);
	} else {
		std::snprintf(msg, sizeof(msg), "quyen194-item-sorting: [%s] the game rejected the sort (result %u) before any move. Nothing was moved.",
			section, static_cast<unsigned>(txnResult));
	}
	ctx->LogWarn(msg);
	return txnResult;
}

void __cdecl DoSortGrid(const PluginContext* ctx, void* userData) noexcept {
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;
	namespace Grid      = Quyen194ItemSorting::Grid;

	const auto target = static_cast<GridTarget>(reinterpret_cast<uintptr_t>(userData));
	const auto info   = InfoFor(target);
	const std::string tag = std::string("quyen194-item-sorting: [") + info.section + "] ";

	// Hot reload: every setting of every section, `enabled` included. Also
	// refreshes mod_data_path's classification tables (see LoadConfig).
	const Config fresh = LoadConfig(ctx);
	if (!fresh.fileError.empty()) {
		ctx->LogWarn((tag + "sort skipped -- quyen194-item-sorting.toml is not valid TOML: " + fresh.fileError).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortConfigError);
		return;
	}
	const Grid::ParsedSection& section = SectionFor(fresh, target);
	for (const auto& w : section.warnings) {
		ctx->LogWarn(("quyen194-item-sorting: " + w).c_str());
	}
	if (!section.enabled) {
		ctx->LogInfo((tag + "sort skipped -- enabled = false.").c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortDisabled);
		return;
	}
	if (!section.errors.empty()) {
		for (const auto& e : section.errors) {
			ctx->LogWarn(("quyen194-item-sorting: config error: " + e).c_str());
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
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortRejected);
		return;
	}

	const Grid::Plan plan = Grid::PlanSort(state.facts, section.config);
	for (const auto& line : plan.log) {
		ctx->LogInfo(("quyen194-item-sorting: plan: " + line).c_str());
	}
	if (plan.status == Grid::Status::ConfigError) {
		ctx->LogWarn((tag + "config error: " + plan.error).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortConfigError);
		return;
	}
	if (plan.status == Grid::Status::NoRoom) {
		ctx->LogWarn((tag + "sort failed, nothing moved: " + plan.error).c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::GridSortNoRoom);
		return;
	}

	uint32_t   committed = 0;
	const auto txnResult = ApplyGridMoves(ctx, player, info.container, plan.moves, state.facts, info.section, &committed);

	char summary[320];
	std::snprintf(summary, sizeof(summary),
		"%sarranged %u item(s) (%u fixed in place, %u outside the grid%s); %zu move(s) planned, %u committed (result %u).", tag.c_str(),
		plan.itemsArranged, plan.itemsFixed, plan.itemsOutsideGrid, plan.compactRetryUsed ? ", needed the compact retry" : "", plan.moves.size(),
		committed, static_cast<unsigned>(txnResult));
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
		std::snprintf(buf, sizeof(buf), "quyen194-item-sorting: on-screen shared page is now %u (%s).", page + 1, reason);
		ctx->LogInfo(buf);
	}
}

void MarkPageUnknown(const PluginContext* ctx, const char* reason) {
	if (!g_selectedTabKnown.exchange(false)) {
		return;
	}
	if (ctx != nullptr) {
		const std::string msg = std::string("quyen194-item-sorting: on-screen shared page is unknown until an item on it is hovered (") + reason + ").";
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

// --- UI message listener (button clicks) --------------------------------

auto __cdecl OnUiMessage(const PluginContext* ctx, const SharedEvents::UiMessageEvent* event, void*) noexcept -> SharedEvents::UiMessageAction {
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;

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
		std::snprintf(buf, sizeof(buf), "quyen194-item-sorting: debug: UiMessage target=\"%s\" command=\"%s\" text=\"%s\"",
		               event->target != nullptr ? event->target : "(null)",
		               event->command != nullptr ? event->command : "(null)",
		               event->text != nullptr ? event->text : "(null)");
		ctx->LogInfo(buf);
	}

	if (event->target != nullptr && event->command != nullptr) {
		TrackStashNavigation(ctx, event->target, event->command, event->text);
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
		{ "quyen194-item-sorting/ActionAutoSort", g_actionAutoSortPanel },
		{ "quyen194-item-sorting/ActionAddBlankPage", g_actionAddBlankPagePanel },
		{ "quyen194-item-sorting/ActionRemoveCurrentPage", g_actionRemoveCurrentPagePanel },
		{ "quyen194-item-sorting/ActionSortPersonalStash", g_actionSortPersonalStashPanel },
		{ "quyen194-item-sorting/ActionSortInventory", g_actionSortInventoryPanel },
		{ "quyen194-item-sorting/ActionSortCube", g_actionSortCubePanel },
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
	if (std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionSortPersonalStash") == 0) {
		gridTarget = GridTarget::PersonalStash;
	} else if (std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionSortInventory") == 0) {
		gridTarget = GridTarget::Inventory;
	} else if (std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionSortCube") == 0) {
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

	if (std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionAutoSort") == 0) {
		if (!g_config.enabled) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::AutoSortDisabled);
		} else if (g_threads->runOnGameThread(ctx, DoAutoSort, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if ((std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionAddBlankPage") == 0
				   || std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionRemoveCurrentPage") == 0)
			   && !g_selectedTabKnown.load()) {
		ctx->LogInfo("quyen194-item-sorting: page action skipped -- the on-screen shared page is unknown (hover an item on it first).");
		ShowToast(ctx, UiLayouts::OutcomeMessage::PageUnknown);
	} else if (std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionAddBlankPage") == 0) {
		if (g_threads->runOnGameThread(ctx, DoAddBlankPage, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if (std::strcmp(matched->logicalName, "quyen194-item-sorting/ActionRemoveCurrentPage") == 0) {
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
	namespace UiLayouts = Quyen194ItemSorting::UiLayouts;

	if (context == nullptr) {
		return false;
	}

	if (context->QueryService(&g_inventory) != ServiceQueryResult::Success || !HasInventoryServiceField(g_inventory, InventoryServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: InventoryService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_items) != ServiceQueryResult::Success || !HasItemServiceField(g_items, ItemServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: ItemService is unavailable.");
		return false;
	}
	if (!HasItemServiceCapability(g_items, Items::ItemServiceCapability::SharedStashWrite)) {
		context->LogError("quyen194-item-sorting: this D2RLoader build does not support shared-stash item writes (needs ItemServiceCapability::SharedStashWrite).");
		return false;
	}
	if (context->QueryService(&g_threads) != ServiceQueryResult::Success || !HasThreadServiceField(g_threads, ThreadServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: ThreadService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_panels) != ServiceQueryResult::Success || !HasPanelServiceField(g_panels, PanelServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: PanelService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_resources) != ServiceQueryResult::Success || !HasResourceServiceField(g_resources, ResourceServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: ResourceService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_widgets) != ServiceQueryResult::Success || !HasWidgetServiceField(g_widgets, WidgetServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: WidgetService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_sharedEvents) != ServiceQueryResult::Success || !HasSharedEventServiceField(g_sharedEvents, SharedEventServiceRequiredSize)) {
		context->LogError("quyen194-item-sorting: SharedEventService is unavailable.");
		return false;
	}

	g_config = LoadConfig(context);

	// Toast panel (built at runtime -- see ui_layouts.hpp).
	const std::string toastJson = UiLayouts::BuildToastLayoutJson();
	const Resources::ResourceRegistration toastResource {
		.structSize = Resources::ResourceRegistrationSize,
		.path       = "data/global/ui/layouts/quyen194-item-sorting/Toasthd.json",
		.bytes      = toastJson.data(),
		.byteCount  = toastJson.size(),
	};
	Resources::RegistrationHandle toastResourceHandle = Resources::InvalidHandle;
	if (g_resources->registerResource(context, &toastResource, &toastResourceHandle) != Resources::Result::Success) {
		context->LogError("quyen194-item-sorting: could not register the toast layout resource.");
		return false;
	}

	const Panels::PanelRegistration toastRegistration {
		.structSize = Panels::PanelRegistrationSize,
		.flags      = Panels::PanelFlags::CloseOnEscape,
		.localId    = "Toast",
	};
	if (g_panels->registerPanel(context, &toastRegistration, &g_toastPanel) != Panels::Result::Success) {
		context->LogError("quyen194-item-sorting: could not register the toast panel.");
		return false;
	}

	// Invisible trigger panels, one per button -- see the g_action*Panel
	// declarations near the top of this file for why. Each layout file is
	// data/global/ui/layouts/quyen194-item-sorting/Action<Name>hd.json in the
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
			context->LogError("quyen194-item-sorting: could not register a button trigger panel.");
			return false;
		}
	}

	// The inventory's Sort button: a plugin child layout on the stock
	// PlayerInventory panel. Its file is data/global/ui/layouts/
	// quyen194-item-sorting/InventoryToolshd.json in the companion mpq (a
	// single ButtonWidget root -- see that file). Keyboard/mouse only: its
	// position is in mouse-layout coordinates. Not fatal if it fails: every
	// other button still works.
	const Panels::ChildLayoutRegistration inventoryTools {
		.structSize = Panels::ChildLayoutRegistrationSize,
		.flags      = Panels::ChildLayoutFlags::KeyboardMouseOnly,
		.stockPanel = Panels::StockPanel::PlayerInventory,
		.localId    = "InventoryTools",
	};
	if (g_panels->registerChildLayout(context, &inventoryTools, &g_inventoryToolsChildLayout) != Panels::Result::Success) {
		context->LogWarn("quyen194-item-sorting: could not add the Sort button to the inventory panel; inventory sorting has no button.");
	}

	const SharedEvents::UiMessageListener listener {
		.structSize = SharedEvents::UiMessageListenerSize,
		.callback   = OnUiMessage,
	};
	if (g_sharedEvents->registerUiMessageListener(context, &listener, &g_uiMessageListener) != SharedEvents::Result::Success) {
		context->LogError("quyen194-item-sorting: could not register the UI message listener.");
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
		context->LogWarn("quyen194-item-sorting: could not register the item tooltip listener; the on-screen page can only be set with the page arrows / plugin buttons.");
	}

	context->LogInfo("quyen194-item-sorting: loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	// D2RLoader unregisters every resource/panel/child-layout/listener this
	// plugin owns automatically on unload (see PluginSDK README "Panels,
	// Layouts, and Widgets" / "Shared UI Events"), so there is nothing to do
	// here explicitly -- matches every SDK example.
}
