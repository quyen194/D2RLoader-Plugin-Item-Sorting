// Remote Stash Sorting -- D2RLoader plugin entry point.
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

#include "item_classification.hpp"
#include "sort_engine.hpp"
#include "ui_layouts.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace D2RL;

namespace {

constexpr PluginInfo kPluginInfo {
	.infoSize    = PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = RemoteStashSorting::UiLayouts::kPluginId,
	.name        = "Remote Stash Sorting",
	.version     = "0.1.0",
	.author      = "quyen19492",
	.description = "Auto-sorts the shared stash and rotates blank tabs.",
	.flags       = PluginFlags::Shared,
};

const InventoryService*   g_inventory    = nullptr;
const ItemService*        g_items        = nullptr;
const ThreadService*      g_threads      = nullptr;
const PanelService*       g_panels       = nullptr;
const ResourceService*    g_resources    = nullptr;
const WidgetService*      g_widgets      = nullptr;
const SharedEventService* g_sharedEvents = nullptr;

Panels::ChildLayoutHandle   g_buttonsChildLayout = Panels::InvalidChildLayoutHandle;
Panels::RegistrationHandle  g_toastPanel         = Panels::InvalidHandle;
SharedEvents::ListenerHandle g_uiMessageListener = SharedEvents::InvalidHandle;

// The SDK has no query for "which shared-stash tab is on screen right now"
// or "how many active shared-stash tabs exist" (see README "Known gaps").
// v1 works around the first gap by having the plugin track its own
// "selected tab" pointer (moved with the < Tab / Tab > buttons, shown in the
// toast) instead of the tab D2R itself has open; the second gap is worked
// around with the shared_stash_tab_count config value below, which the user
// must set to match their actual stash.
std::atomic<uint32_t> g_selectedTab { 0 };

struct Config {
	bool                                        enabled    = true;
	uint32_t                                    gridWidth  = 16;
	uint32_t                                    gridHeight = 13;
	uint32_t                                    tabCount   = 20;
	std::vector<RemoteStashSorting::CategorySpec> layout;
	// Debug aid for the "Known gaps" #6/#7 live-layout work: when true, every
	// incoming UiMessageEvent (ours and every other plugin's/D2R's own) is
	// logged as target/command, so the exact native message D2R fires when
	// the Shared Stash panel opens/closes/switches tabs can be found by
	// watching the log while toggling it in-game. Off by default -- this is
	// a firehose, not something to leave on.
	bool                                        debugLogUiMessages = false;
};

Config g_config;

Config LoadConfig(const PluginContext* ctx) {
	Config cfg;
	cfg.layout = RemoteStashSorting::DefaultLayout();
	if (ctx == nullptr) {
		return cfg;
	}

	std::vector<char> buffer(16 * 1024, '\0');
	uint32_t          required = 0;
	if (!ctx->ReadConfig(buffer.data(), static_cast<uint32_t>(buffer.size()), &required)) {
		ctx->LogWarn("remote-stash-sorting: could not read remote-stash-sorting.toml; using built-in defaults.");
		return cfg;
	}
	if (required > buffer.size()) {
		ctx->LogWarn("remote-stash-sorting: config file is larger than expected; using built-in defaults.");
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
		const std::string msg = std::string("remote-stash-sorting: config parse error: ") + std::string(err.description());
		ctx->LogWarn(msg.c_str());
		return cfg;
	}

	auto section = parsed["remote-stash-sorting"];
	cfg.enabled            = section["enabled"].value_or(true);
	cfg.tabCount           = section["shared_stash_tab_count"].value_or(20u);
	cfg.gridWidth          = section["grid"]["width"].value_or(16u);
	cfg.gridHeight         = section["grid"]["height"].value_or(13u);
	cfg.debugLogUiMessages = section["debug_log_ui_messages"].value_or(false);

	if (auto layoutArr = section["layout"].as_array()) {
		std::vector<RemoteStashSorting::CategorySpec> parsedLayout;
		bool                                           allKnown = true;
		for (auto&& node : *layoutArr) {
			const auto* row = node.as_table();
			if (row == nullptr) {
				continue;
			}
			std::string modeId  = (*row)["mode_id"].value_or(std::string {});
			uint32_t    minTabs = (*row)["min_tabs"].value_or(0u);
			if (modeId.empty() || !RemoteStashSorting::IsKnownModeId(modeId)) {
				allKnown = false;
				break;
			}
			parsedLayout.push_back({ modeId, minTabs });
		}
		if (allKnown && !parsedLayout.empty()) {
			cfg.layout = std::move(parsedLayout);
		} else if (!allKnown) {
			ctx->LogWarn("remote-stash-sorting: config has an unknown mode_id in [[layout]]; using the built-in default layout instead.");
		}
	}

	return cfg;
}

// --- Reading the live shared stash into ItemFacts --------------------------

struct EnumState {
	std::vector<RemoteStashSorting::ItemFacts> facts;
};

auto __cdecl CollectSharedStashItem(const PluginContext*, const Items::ItemInfo* item, void* userData) noexcept -> Inventory::IterationAction {
	auto* state = static_cast<EnumState*>(userData);
	if (state == nullptr || item == nullptr || item->structSize < Items::ItemInfoRequiredSize) {
		return Inventory::IterationAction::Continue;
	}
	if (item->container != Items::ItemContainer::SharedStash) {
		return Inventory::IterationAction::Continue;
	}

	RemoteStashSorting::ItemFacts fact;
	fact.handle          = item->handle;
	fact.code            = item->code;
	fact.quality         = static_cast<uint32_t>(item->quality);
	fact.itemLevel       = item->itemLevel;
	fact.sharedStashPage = item->sharedStashPage;
	fact.x               = item->x;
	fact.y               = item->y;

	if (auto cls = RemoteStashSorting::Classification::Classify(item->code)) {
		fact.w           = cls->width;
		fact.h           = cls->height;
		fact.isGear       = cls->isGear;
		fact.isTwoHanded  = cls->isTwoHanded;
		fact.gearSlot     = cls->gearSlot;
		fact.nonGearKind  = cls->nonGearKind;
		fact.sortLevel    = cls->baseLevelReq;
	}
	// v1 scope note (README "Known gaps"): sortLevel above is only the base
	// item's own levelreq. The save-editor's _ssItemSortLevel also raises it
	// using UniqueItems/SetItems "lvl req" and the item's rolled affix
	// levelreq, both of which need row layouts DataTableService does not
	// publish (see data_tables.h -- rows are untyped bytes). itemLevel
	// (below) is the primary tie-break ahead of sortLevel in every mode, so
	// this only affects ordering among same-itemLevel items.

	if (item->quality == Items::Quality::Set) {
		fact.setId        = item->qualityRecordId;
		fact.setGroupName = RemoteStashSorting::Classification::SetGroupForRow(item->qualityRecordId);
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
			"remote-stash-sorting: shared-stash scan: %zu item(s) across %zu distinct tab number(s) [%s]. If this "
			"stays the SAME single tab number no matter which tab is open in-game when you click a button, "
			"forEachInventoryItem is only returning the currently open tab, not every tab (see README Known gaps #8).",
			state->facts.size(),
			tabs.size(),
			tabList.c_str());
		ctx->LogInfo(buf);
	}

	return ok;
}

// --- Turning a plan into an item transaction --------------------------------

auto ApplyMoves(const PluginContext* ctx, PlayerHandle player, const std::vector<RemoteStashSorting::PlannedMove>& moves, uint32_t* committedCount) -> Items::Result {
	if (committedCount != nullptr) {
		*committedCount = 0;
	}
	if (moves.empty()) {
		return Items::Result::Success;
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

	if (ops.size() > Items::MaxExistingItemOperations) {
		ctx->LogWarn(
			"remote-stash-sorting: plan exceeds 4096 operations; it will be sent in several transactions and is NOT "
			"atomic across all of them (see README).");
	}

	uint32_t totalCommitted = 0;
	size_t   offset         = 0;
	while (offset < ops.size()) {
		const size_t chunkSize = std::min<size_t>(Items::MaxExistingItemOperations, ops.size() - offset);

		Items::ExistingItemTransaction txn {};
		txn.structSize      = Items::ExistingItemTransactionSize;
		txn.player           = player;
		txn.operationCount   = static_cast<uint32_t>(chunkSize);
		txn.operations       = ops.data() + offset;

		Items::ExistingItemTransactionResult result {};
		result.structSize = Items::ExistingItemTransactionResultSize;

		const auto txnResult = g_items->executeExistingItemTransaction(ctx, &txn, &result);
		if (txnResult != Items::Result::Success) {
			if (committedCount != nullptr) {
				*committedCount = totalCommitted;
			}
			return txnResult;
		}

		totalCommitted += result.committedOperationCount;
		offset += chunkSize;
	}

	if (committedCount != nullptr) {
		*committedCount = totalCommitted;
	}
	return Items::Result::Success;
}

// --- Toast -------------------------------------------------------------

void ShowToast(const PluginContext* ctx, RemoteStashSorting::UiLayouts::OutcomeMessage message) {
	namespace UiLayouts = RemoteStashSorting::UiLayouts;
	if (ctx == nullptr || g_panels == nullptr || g_widgets == nullptr) {
		return;
	}
	if (g_panels->openPanel(ctx, g_toastPanel) != Panels::Result::Success) {
		return;
	}

	Widgets::WidgetHandle panelWidget = Widgets::InvalidHandle;
	if (g_widgets->findPanel(ctx, "remote-stash-sorting/Toast", &panelWidget) != Widgets::Result::Success) {
		return;
	}

	for (uint32_t i = 0; i < static_cast<uint32_t>(UiLayouts::OutcomeMessage::Count); ++i) {
		const auto            candidate = static_cast<UiLayouts::OutcomeMessage>(i);
		Widgets::WidgetHandle w         = Widgets::InvalidHandle;
		if (g_widgets->findWidget(ctx, panelWidget, UiLayouts::OutcomeMessageWidgetName(candidate), &w) == Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(ctx, w, candidate == message);
		}
	}

	const uint32_t selected = g_selectedTab.load();
	for (uint32_t t = 0; t <= UiLayouts::kMaxDisplayableTab + 1; ++t) {
		const bool        isOverflowSlot = (t == UiLayouts::kMaxDisplayableTab + 1);
		const std::string name           = isOverflowSlot ? std::string("MsgTabOverflow") : UiLayouts::TabWidgetName(t);
		const bool         shouldShow    = isOverflowSlot ? (selected > UiLayouts::kMaxDisplayableTab) : (t == selected);

		Widgets::WidgetHandle w = Widgets::InvalidHandle;
		if (g_widgets->findWidget(ctx, panelWidget, name.c_str(), &w) == Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(ctx, w, shouldShow);
		}
	}
}

struct ToastRequest {
	RemoteStashSorting::UiLayouts::OutcomeMessage message;
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
void QueueToast(const PluginContext* ctx, RemoteStashSorting::UiLayouts::OutcomeMessage message) {
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
	namespace UiLayouts = RemoteStashSorting::UiLayouts;

	PlayerHandle player = InvalidPlayerHandle;
	if (g_inventory->getLocalPlayer(ctx, &player) != Inventory::Result::Success) {
		ctx->LogWarn("remote-stash-sorting: auto sort needs an authoritative local player.");
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	EnumState state;
	if (!CollectSharedStash(ctx, player, &state)) {
		ctx->LogWarn("remote-stash-sorting: could not enumerate the shared stash.");
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}

	const auto outcome = RemoteStashSorting::RunAutoSort(state.facts, g_config.layout, g_config.tabCount, g_config.gridWidth, g_config.gridHeight);
	if (!outcome.ok) {
		const std::string logMsg = "remote-stash-sorting: auto sort failed: " + outcome.error;
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

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, outcome.moves, &committed);

	char logMsg[320] {};
	std::snprintf(
		logMsg,
		sizeof(logMsg),
		"remote-stash-sorting: auto sort placed %u item(s) across %u tab(s); committed %u/%zu move op(s) (transaction result %u).",
		outcome.itemsSorted,
		outcome.tabsUsed,
		committed,
		outcome.moves.size(),
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
	namespace UiLayouts = RemoteStashSorting::UiLayouts;

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

	const auto outcome = RemoteStashSorting::MoveBlankTabAfter(state.facts, g_selectedTab.load(), g_config.tabCount);
	if (!outcome.ok) {
		const std::string logMsg = "remote-stash-sorting: add blank page failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());
		QueueToast(ctx, UiLayouts::OutcomeMessage::NoBlankTab);
		return;
	}

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, outcome.moves, &committed);
	if (txnResult == Items::Result::NotAuthoritative) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::MultiplayerClientUnsupported);
		return;
	}
	if (txnResult != Items::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}
	g_selectedTab.store(outcome.blankTab);
	QueueToast(ctx, UiLayouts::OutcomeMessage::BlankMoved);
}

void __cdecl DoRemoveCurrentPage(const PluginContext* ctx, void*) noexcept {
	namespace UiLayouts = RemoteStashSorting::UiLayouts;

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

	const auto outcome = RemoteStashSorting::PushBlankTabToEnd(state.facts, g_selectedTab.load(), g_config.tabCount);
	if (!outcome.ok) {
		const std::string logMsg = "remote-stash-sorting: remove current page failed: " + outcome.error;
		ctx->LogWarn(logMsg.c_str());
		const auto msg = outcome.error.find("out of range") != std::string::npos ? UiLayouts::OutcomeMessage::OutOfRange : UiLayouts::OutcomeMessage::CurrentNotEmpty;
		QueueToast(ctx, msg);
		return;
	}

	uint32_t   committed = 0;
	const auto txnResult = ApplyMoves(ctx, player, outcome.moves, &committed);
	if (txnResult == Items::Result::NotAuthoritative) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::MultiplayerClientUnsupported);
		return;
	}
	if (txnResult != Items::Result::Success) {
		QueueToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		return;
	}
	QueueToast(ctx, UiLayouts::OutcomeMessage::PageRemoved);
}

// --- UI message listener (button clicks) --------------------------------

// Debug aid for "Known gaps" #6/#7: logs the live, engine-resolved rect of
// the button strip and each of its five buttons. WidgetService::getWidgetRect
// reports coordinates local to each widget's parent, so this is ground truth
// from the engine instead of another screenshot-pixel guess. Called whenever
// one of our own buttons is clicked (cheap -- user-triggered, not per-frame).
void LogButtonStripRects(const PluginContext* ctx) {
	if (ctx == nullptr || g_widgets == nullptr) {
		return;
	}

	Widgets::WidgetHandle root = Widgets::InvalidHandle;
	if (g_widgets->findPanel(ctx, "remote-stash-sorting/StashTools", &root) != Widgets::Result::Success) {
		ctx->LogInfo("remote-stash-sorting: debug: could not find the StashTools root widget via findPanel.");
		return;
	}

	Widgets::Rect rootRect {};
	if (g_widgets->getWidgetRect(ctx, root, &rootRect) == Widgets::Result::Success) {
		char buf[160];
		std::snprintf(buf, sizeof(buf), "remote-stash-sorting: debug: StashTools rect (local to PlayerInventory) = x=%d y=%d w=%d h=%d",
		               rootRect.x, rootRect.y, rootRect.width, rootRect.height);
		ctx->LogInfo(buf);
	} else {
		ctx->LogInfo("remote-stash-sorting: debug: getWidgetRect failed for StashTools root.");
	}

	static constexpr const char* kButtonNames[] = {
		"AutoSortButton", "AddBlankPageButton", "RemoveCurrentPageButton", "PrevTabButton", "NextTabButton",
	};
	for (const char* name : kButtonNames) {
		Widgets::WidgetHandle w = Widgets::InvalidHandle;
		if (g_widgets->findWidget(ctx, root, name, &w) != Widgets::Result::Success) {
			continue;
		}
		Widgets::Rect r {};
		if (g_widgets->getWidgetRect(ctx, w, &r) == Widgets::Result::Success) {
			char buf[160];
			std::snprintf(buf, sizeof(buf), "remote-stash-sorting: debug: %s rect (local to StashTools) = x=%d y=%d w=%d h=%d",
			               name, r.x, r.y, r.width, r.height);
			ctx->LogInfo(buf);
		}
	}
}

auto __cdecl OnUiMessage(const PluginContext* ctx, const SharedEvents::UiMessageEvent* event, void*) noexcept -> SharedEvents::UiMessageAction {
	namespace UiLayouts = RemoteStashSorting::UiLayouts;

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
		std::snprintf(buf, sizeof(buf), "remote-stash-sorting: debug: UiMessage target=\"%s\" command=\"%s\" text=\"%s\"",
		               event->target != nullptr ? event->target : "(null)",
		               event->command != nullptr ? event->command : "(null)",
		               event->text != nullptr ? event->text : "(null)");
		ctx->LogInfo(buf);
	}

	if (event->target == nullptr || event->command == nullptr) {
		return SharedEvents::UiMessageAction::Continue;
	}
	if (std::strcmp(event->target, UiLayouts::kPluginId) != 0) {
		return SharedEvents::UiMessageAction::Continue;
	}

	LogButtonStripRects(ctx);

	if (std::strcmp(event->command, "AutoSort") == 0) {
		if (!g_config.enabled) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::AutoSortDisabled);
		} else if (g_threads->runOnGameThread(ctx, DoAutoSort, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if (std::strcmp(event->command, "AddBlankPage") == 0) {
		if (g_threads->runOnGameThread(ctx, DoAddBlankPage, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if (std::strcmp(event->command, "RemoveCurrentPage") == 0) {
		if (g_threads->runOnGameThread(ctx, DoRemoveCurrentPage, nullptr) != Threads::Result::Success) {
			ShowToast(ctx, UiLayouts::OutcomeMessage::InternalError);
		}
	} else if (std::strcmp(event->command, "SelectPrevTab") == 0) {
		const uint32_t current = g_selectedTab.load();
		g_selectedTab.store(current == 0 ? 0 : current - 1);
		ShowToast(ctx, UiLayouts::OutcomeMessage::TabSelected);
	} else if (std::strcmp(event->command, "SelectNextTab") == 0) {
		const uint32_t current = g_selectedTab.load();
		const uint32_t maxTab  = g_config.tabCount == 0 ? 0 : g_config.tabCount - 1;
		g_selectedTab.store(current >= maxTab ? maxTab : current + 1);
		ShowToast(ctx, UiLayouts::OutcomeMessage::TabSelected);
	} else {
		return SharedEvents::UiMessageAction::Continue;
	}

	return SharedEvents::UiMessageAction::Consume;
}

} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const PluginInfo* {
	return &kPluginInfo;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const PluginContext* context) noexcept -> bool {
	namespace UiLayouts = RemoteStashSorting::UiLayouts;

	if (context == nullptr) {
		return false;
	}

	if (context->QueryService(&g_inventory) != ServiceQueryResult::Success || !HasInventoryServiceField(g_inventory, InventoryServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: InventoryService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_items) != ServiceQueryResult::Success || !HasItemServiceField(g_items, ItemServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: ItemService is unavailable.");
		return false;
	}
	if (!HasItemServiceCapability(g_items, Items::ItemServiceCapability::SharedStashWrite)) {
		context->LogError("remote-stash-sorting: this D2RLoader build does not support shared-stash item writes (needs ItemServiceCapability::SharedStashWrite).");
		return false;
	}
	if (context->QueryService(&g_threads) != ServiceQueryResult::Success || !HasThreadServiceField(g_threads, ThreadServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: ThreadService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_panels) != ServiceQueryResult::Success || !HasPanelServiceField(g_panels, PanelServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: PanelService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_resources) != ServiceQueryResult::Success || !HasResourceServiceField(g_resources, ResourceServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: ResourceService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_widgets) != ServiceQueryResult::Success || !HasWidgetServiceField(g_widgets, WidgetServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: WidgetService is unavailable.");
		return false;
	}
	if (context->QueryService(&g_sharedEvents) != ServiceQueryResult::Success || !HasSharedEventServiceField(g_sharedEvents, SharedEventServiceRequiredSize)) {
		context->LogError("remote-stash-sorting: SharedEventService is unavailable.");
		return false;
	}

	g_config = LoadConfig(context);

	// Button strip, attached to the stock PlayerInventory panel. Its JSON
	// (coordinates, sprite references, tooltip string-table keys) and its
	// button_mold/charminvbutton sprites all live as real files in the
	// d2rl-remote-stash-sorting.mpq companion folder beside this DLL now
	// (see ui_layouts.hpp "Child layout" comment) -- D2RCore discovers a
	// "d2rl-<plugin-id>.mpq" companion and its
	// "data/global/ui/layouts/<plugin-id>/<local-id>hd.json" layout on its
	// own (see PluginSDK README "Resources and Companion MPQs" and
	// CMakeLists.txt), so no ResourceService::registerResource call is
	// needed here for it -- just attach it by localId below.
	const Panels::ChildLayoutRegistration buttonsRegistration {
		.structSize = Panels::ChildLayoutRegistrationSize,
		.stockPanel = Panels::StockPanel::PlayerInventory,
		.localId    = "StashTools",
	};
	if (g_panels->registerChildLayout(context, &buttonsRegistration, &g_buttonsChildLayout) != Panels::Result::Success) {
		context->LogError("remote-stash-sorting: could not register the button strip on PlayerInventory.");
		return false;
	}

	// Toast panel (built at runtime -- see ui_layouts.hpp).
	const std::string toastJson = UiLayouts::BuildToastLayoutJson();
	const Resources::ResourceRegistration toastResource {
		.structSize = Resources::ResourceRegistrationSize,
		.path       = "data/global/ui/layouts/remote-stash-sorting/Toasthd.json",
		.bytes      = toastJson.data(),
		.byteCount  = toastJson.size(),
	};
	Resources::RegistrationHandle toastResourceHandle = Resources::InvalidHandle;
	if (g_resources->registerResource(context, &toastResource, &toastResourceHandle) != Resources::Result::Success) {
		context->LogError("remote-stash-sorting: could not register the toast layout resource.");
		return false;
	}

	const Panels::PanelRegistration toastRegistration {
		.structSize = Panels::PanelRegistrationSize,
		.flags      = Panels::PanelFlags::CloseOnEscape,
		.localId    = "Toast",
	};
	if (g_panels->registerPanel(context, &toastRegistration, &g_toastPanel) != Panels::Result::Success) {
		context->LogError("remote-stash-sorting: could not register the toast panel.");
		return false;
	}

	const SharedEvents::UiMessageListener listener {
		.structSize = SharedEvents::UiMessageListenerSize,
		.callback   = OnUiMessage,
	};
	if (g_sharedEvents->registerUiMessageListener(context, &listener, &g_uiMessageListener) != SharedEvents::Result::Success) {
		context->LogError("remote-stash-sorting: could not register the UI message listener.");
		return false;
	}

	context->LogInfo("remote-stash-sorting: loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	// D2RLoader unregisters every resource/panel/child-layout/listener this
	// plugin owns automatically on unload (see PluginSDK README "Panels,
	// Layouts, and Widgets" / "Shared UI Events"), so there is nothing to do
	// here explicitly -- matches every SDK example.
}
