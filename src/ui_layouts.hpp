#pragma once

// JSON panel/child-layout sources for the Remote Stash Sorting plugin.
//
// Design note (v1 scope, see README "Known gaps" -- flagged for sign-off):
// D2RLPlugin::WidgetService has no "set widget text" call (only
// getInputText, which reads InputTextBoxWidget content, and
// setWidgetVisible/setWidgetEnabled). There is therefore no supported way to
// push freshly-computed text (an item count, an error detail) into an
// already-registered JSON widget at runtime. The toast panel below works
// around this the only way the SDK allows: every possible message is a
// separate TextBoxWidget baked into the JSON up front, and showing a result
// means calling setWidgetVisible(true) on exactly one of them (and false on
// the rest) rather than setting text. Exact item/tab counts from Auto Sort
// go to the plugin's log file (PluginContext::LogInfo) instead of the toast
// for the same reason -- there are too many possible values to enumerate as
// widgets. The "selected tab" pointer (see plugin.cpp) IS small and bounded
// (kMaxDisplayableTab+1 values) so it gets the enumerated-widget treatment.

#include <cstdint>
#include <string>

namespace RemoteStashSorting::UiLayouts {

inline constexpr const char* kPluginId = "remote-stash-sorting";

// ---------------------------------------------------------------------
// Child layout: a small button strip attached to the stock PlayerInventory
// panel (registerChildLayout(StockPanel::PlayerInventory, ...)). Shared
// Stash is always opened alongside PlayerInventory, so this is visible
// whenever there is a shared stash on screen to act on.
//
// Position is a first guess (top-left corner, stacked vertically) and very
// likely needs adjusting once this is actually seen in-game -- there was no
// way to verify stock PlayerInventory panel dimensions without a live D2R
// session. See README "Known gaps".
inline constexpr char kButtonsChildLayoutJson[] = R"json({
	"type": "Widget", "name": "remote-stash-sorting/StashTools",
	"fields": { "rect": { "x": 0, "y": 0, "width": 1, "height": 1 } },
	"children": [
		{
			"type": "ButtonWidget", "name": "AutoSortButton",
			"fields": {
				"rect": { "x": 4, "y": 4 },
				"filename": "Panel\\Modals\\ModalButton",
				"focusIndicatorFilename": "Controller/HoverImages/ModalButton_Hover",
				"pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
				"textString": "Auto Sort",
				"pointSize": "$SmallFontSize",
				"textColor": "$FontColorWhite",
				"onClickMessage": "remote-stash-sorting:AutoSort"
			}
		},
		{
			"type": "ButtonWidget", "name": "AddBlankPageButton",
			"fields": {
				"rect": { "x": 4, "y": 38 },
				"filename": "Panel\\Modals\\ModalButton",
				"focusIndicatorFilename": "Controller/HoverImages/ModalButton_Hover",
				"pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
				"textString": "Add Blank Page",
				"pointSize": "$SmallFontSize",
				"textColor": "$FontColorWhite",
				"onClickMessage": "remote-stash-sorting:AddBlankPage"
			}
		},
		{
			"type": "ButtonWidget", "name": "RemoveCurrentPageButton",
			"fields": {
				"rect": { "x": 4, "y": 72 },
				"filename": "Panel\\Modals\\ModalButton",
				"focusIndicatorFilename": "Controller/HoverImages/ModalButton_Hover",
				"pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
				"textString": "Remove Current Page",
				"pointSize": "$SmallFontSize",
				"textColor": "$FontColorWhite",
				"onClickMessage": "remote-stash-sorting:RemoveCurrentPage"
			}
		},
		{
			"type": "ButtonWidget", "name": "PrevTabButton",
			"fields": {
				"rect": { "x": 4, "y": 110 },
				"filename": "Panel\\Modals\\ModalButton",
				"focusIndicatorFilename": "Controller/HoverImages/ModalButton_Hover",
				"pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
				"textString": "< Tab",
				"pointSize": "$SmallFontSize",
				"textColor": "$FontColorWhite",
				"onClickMessage": "remote-stash-sorting:SelectPrevTab"
			}
		},
		{
			"type": "ButtonWidget", "name": "NextTabButton",
			"fields": {
				"rect": { "x": 78, "y": 110 },
				"filename": "Panel\\Modals\\ModalButton",
				"focusIndicatorFilename": "Controller/HoverImages/ModalButton_Hover",
				"pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
				"textString": "Tab >",
				"pointSize": "$SmallFontSize",
				"textColor": "$FontColorWhite",
				"onClickMessage": "remote-stash-sorting:SelectNextTab"
			}
		}
	]
})json";

// ---------------------------------------------------------------------
// Toast panel: the plugin's own result notification, per the user's explicit
// choice of "plugin's own panel/toast" over D2RLoader's console or the D2R
// chat box (see README). Built at runtime because of the enumerated
// tab-number widgets (see file comment above); the outcome-message names and
// the tab-number range are shared with plugin.cpp so the two stay in sync.

enum class OutcomeMessage : uint32_t {
	AutoSortOk = 0,
	AutoSortDisabled,
	AutoSortNotEnoughTabs,
	AutoSortNoRoom,
	AutoSortSetNoRoom,
	AutoSortUnknownMode,
	BlankMoved,
	NoBlankTab,
	PageRemoved,
	CurrentNotEmpty,
	OutOfRange,
	MultiplayerClientUnsupported,
	TabSelected,
	InternalError,
	Count, // not a real message; array size marker
};

inline const char* OutcomeMessageWidgetName(OutcomeMessage msg) {
	switch (msg) {
		case OutcomeMessage::AutoSortOk: return "MsgAutoSortOk";
		case OutcomeMessage::AutoSortDisabled: return "MsgAutoSortDisabled";
		case OutcomeMessage::AutoSortNotEnoughTabs: return "MsgAutoSortNotEnoughTabs";
		case OutcomeMessage::AutoSortNoRoom: return "MsgAutoSortNoRoom";
		case OutcomeMessage::AutoSortSetNoRoom: return "MsgAutoSortSetNoRoom";
		case OutcomeMessage::AutoSortUnknownMode: return "MsgAutoSortUnknownMode";
		case OutcomeMessage::BlankMoved: return "MsgBlankMoved";
		case OutcomeMessage::NoBlankTab: return "MsgNoBlankTab";
		case OutcomeMessage::PageRemoved: return "MsgPageRemoved";
		case OutcomeMessage::CurrentNotEmpty: return "MsgCurrentNotEmpty";
		case OutcomeMessage::OutOfRange: return "MsgOutOfRange";
		case OutcomeMessage::MultiplayerClientUnsupported: return "MsgMultiplayerClientUnsupported";
		case OutcomeMessage::TabSelected: return "MsgTabSelected";
		case OutcomeMessage::InternalError:
		case OutcomeMessage::Count: return "MsgInternalError";
	}
	return "MsgInternalError";
}

inline const char* OutcomeMessageText(OutcomeMessage msg) {
	switch (msg) {
		case OutcomeMessage::AutoSortOk: return "Auto Sort: done. See the plugin log for item/tab counts.";
		case OutcomeMessage::AutoSortDisabled: return "Auto Sort: disabled in remote-stash-sorting.toml.";
		case OutcomeMessage::AutoSortNotEnoughTabs: return "Auto Sort failed: not enough shared-stash tabs for the configured layout.";
		case OutcomeMessage::AutoSortNoRoom: return "Auto Sort failed: a category needs more room than its reserved tabs.";
		case OutcomeMessage::AutoSortSetNoRoom: return "Auto Sort failed: a set doesn't fit in its reserved tabs.";
		case OutcomeMessage::AutoSortUnknownMode: return "Auto Sort failed: unknown category in remote-stash-sorting.toml.";
		case OutcomeMessage::BlankMoved: return "Add Blank Page: moved the blank tab next to the selected tab.";
		case OutcomeMessage::NoBlankTab: return "Add Blank Page failed: no empty tab found after the selected tab.";
		case OutcomeMessage::PageRemoved: return "Remove Current Page: moved the empty tab to the end.";
		case OutcomeMessage::CurrentNotEmpty: return "Remove Current Page failed: the selected tab is not empty.";
		case OutcomeMessage::OutOfRange: return "Failed: the selected tab is out of range.";
		case OutcomeMessage::MultiplayerClientUnsupported:
			return "Failed: item moves as a TCP/IP client aren't supported yet (host-only in this version).";
		case OutcomeMessage::TabSelected: return "Selected tab changed -- see the number below.";
		case OutcomeMessage::InternalError:
		case OutcomeMessage::Count: return "Failed: unexpected error. See the plugin log.";
	}
	return "Failed: unexpected error. See the plugin log.";
}

// Generous enough for any realistic shared-stash tab count; beyond this the
// toast just shows "tab 24+" rather than growing the widget list further.
inline constexpr uint32_t kMaxDisplayableTab = 23;

inline std::string TabWidgetName(uint32_t tab) {
	return tab <= kMaxDisplayableTab ? ("MsgTab" + std::to_string(tab)) : std::string("MsgTabOverflow");
}

// Builds the full toast panel JSON, including one TextBoxWidget per
// OutcomeMessage and one per displayable tab number (all hidden by default;
// plugin.cpp shows exactly one from each group before opening the panel).
inline std::string BuildToastLayoutJson() {
	std::string json;
	json.reserve(16 * 1024);
	json += R"json({
	"type": "Panel", "name": "remote-stash-sorting/Toast",
	"fields": { "anchor": { "x": 0.5, "y": 0.22 }, "priority": 9000 },
	"children": [
		{
			"type": "ImageWidget", "name": "Frame",
			"fields": { "rect": { "x": -320, "y": -70 }, "filename": "Panel\\Modals\\Modal_No_Title_BG" },
			"children": [
				{
					"type": "TextBoxWidget", "name": "Heading",
					"fields": {
						"rect": { "x": 24, "y": 14, "width": 592, "height": 30 },
						"text": "Remote Stash Sorting", "style": "$StyleSettingsTitle"
					}
				},
)json";

	for (uint32_t i = 0; i < static_cast<uint32_t>(OutcomeMessage::Count); ++i) {
		const auto msg = static_cast<OutcomeMessage>(i);
		json += "\t\t\t\t{ \"type\": \"TextBoxWidget\", \"name\": \"";
		json += OutcomeMessageWidgetName(msg);
		json += "\", \"fields\": { \"rect\": { \"x\": 24, \"y\": 52, \"width\": 592, \"height\": 60 }, \"visible\": false, "
		        "\"text\": \"";
		json += OutcomeMessageText(msg);
		json += "\", \"style\": \"$StyleModalDialogDescription\" } },\n";
	}

	for (uint32_t t = 0; t <= kMaxDisplayableTab + 1; ++t) {
		const bool  overflow = t == kMaxDisplayableTab + 1;
		std::string label    = overflow ? "Selected tab: 24+" : ("Selected tab: " + std::to_string(t));
		json += "\t\t\t\t{ \"type\": \"TextBoxWidget\", \"name\": \"";
		json += overflow ? std::string("MsgTabOverflow") : TabWidgetName(t);
		json += "\", \"fields\": { \"rect\": { \"x\": 24, \"y\": 100, \"width\": 300, \"height\": 26 }, \"visible\": false, "
		        "\"text\": \"";
		json += label;
		json += "\", \"style\": \"$StyleModalDialogDescription\" } },\n";
	}

	json += R"json(
				{
					"type": "ButtonWidget", "name": "CloseButton",
					"fields": {
						"rect": { "x": 260, "y": 132 },
						"filename": "Panel\\Modals\\ModalButton",
						"focusIndicatorFilename": "Controller/HoverImages/ModalButton_Hover",
						"pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
						"textString": "@d2r:strOK",
						"pointSize": "$MediumFontSize",
						"textColor": "$FontColorWhite",
						"onClickMessage": "PanelManager:ClosePanel:remote-stash-sorting/Toast"
					}
				}
			]
		}
	]
})json";
	return json;
}

}
