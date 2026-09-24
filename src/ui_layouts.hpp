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
// panel (registerChildLayout(StockPanel::PlayerInventory, localId
// "StashTools")). Shared Stash is always opened alongside PlayerInventory,
// so this is visible whenever there is a shared stash on screen to act on.
//
// Unlike the toast panel below, this layout is fully static (fixed
// coordinates, fixed sprite references, fixed tooltip keys -- nothing
// computed at runtime), so as of iteration 4 it is a real file, not a C++
// string constant:
//
//   d2rl-remote-stash-sorting.mpq/data/global/ui/layouts/remote-stash-sorting/StashToolshd.json
//
// matching the "data/global/ui/layouts/<plugin-id>/<local-id>hd.json"
// convention the PluginSDK README documents, and matching how
// d2rl-charm-inv.mpq ships its own InventoryButtonhd.json. D2RCore resolves
// it from the companion .mpq folder beside the DLL on its own -- plugin.cpp
// no longer calls ResourceService::registerResource for it, only
// PanelService::registerChildLayout to attach it. Its tooltipStrings
// reference "@remote-stash-sorting:<Key>" entries defined in
// d2rl-remote-stash-sorting.mpq/data/local/lng/strings/d2rloader/remote-stash-sorting/strings.json
// (see PluginSDK README "Resources and Companion MPQs" -- confirmed
// supported for tooltipString, not just TextBoxWidget text, by
// d2rl-charm-inv's own CloseButton: "tooltipString": "@d2r:strClose").
//
// Position/size, iteration 3 (see README "Known gaps" #6): reuses the exact
// sprites and edge position the d2rl-charm-inv plugin already ships and
// proves safe in practice -- button_mold.sprite (the small "pouch"
// background) and charminvbutton.sprite (a compact icon button). charm-inv
// attaches its own toggle button at
// { x: 3, y: 672, width: 54, height: 141 } on this exact same
// StockPanel::PlayerInventory child-layout coordinate space, so that spot is
// known-good, empty real estate -- not a guess. Auto Sort reuses that exact
// spot (with the mold graphic behind it, like charm-inv's own button); the
// other four actions stack directly below the mold as plain icon buttons
// with no textString (there is no room for a text label at this size, and
// no "set widget text" SDK call anyway -- see file comment above), each
// with a tooltipString identifying it on hover instead.
//
// Still placeholder in one sense: all five buttons currently use the same
// generic charminvbutton icon (only distinguishable by tooltip). Swap in
// purpose-made icons once the position itself is confirmed working
// in-game -- edit the JSON file directly, no rebuild needed.

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
						"text": "@remote-stash-sorting:ToastTitle", "style": "$StyleSettingsTitle"
					}
				},
)json";

	for (uint32_t i = 0; i < static_cast<uint32_t>(OutcomeMessage::Count); ++i) {
		const auto msg      = static_cast<OutcomeMessage>(i);
		const char* widgetName = OutcomeMessageWidgetName(msg);
		json += "\t\t\t\t{ \"type\": \"TextBoxWidget\", \"name\": \"";
		json += widgetName;
		json += "\", \"fields\": { \"rect\": { \"x\": 24, \"y\": 52, \"width\": 592, \"height\": 60 }, \"visible\": false, "
		        "\"text\": \"@";
		json += kPluginId;
		json += ":";
		// The widget name doubles as its strings.json Key -- see
		// d2rl-remote-stash-sorting.mpq/data/local/lng/strings/d2rloader/
		// remote-stash-sorting/strings.json (one entry per OutcomeMessage,
		// named to match OutcomeMessageWidgetName exactly).
		json += widgetName;
		json += "\", \"style\": \"$StyleModalDialogDescription\" } },\n";
	}

	// Not localized/string-table-backed like the OutcomeMessage loop above --
	// this is a bare number with an English "Selected tab: " prefix, and
	// enumerating kMaxDisplayableTab+2 string-table keys just for that
	// prefix isn't worth it. Revisit if that changes.
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
