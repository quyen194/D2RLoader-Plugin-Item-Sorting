#include "item_classification.hpp"
#include "item_classification_data.hpp"
#include "mod_item_data.hpp"

#include <algorithm>

namespace RemoteStashSorting::Classification {

namespace {
// Set by LoadFromModFolder/UseEmbeddedTable. std::nullopt means "answer from
// the embedded kTable/kSetGroupByRow below" -- the original, always-present
// behavior. This is plain (non-atomic) state: like the rest of this plugin's
// config, it is only ever written from the game thread during config
// (re)load, never read concurrently with a write.
std::optional<ModData::LoadedTable> g_override;
}

std::optional<Facts> Classify(uint32_t packedItemCode) {
	if (g_override) {
		const auto& entries = g_override->entries;
		const auto* begin   = entries.data();
		const auto* end     = entries.data() + entries.size();
		const auto* found   = std::lower_bound(begin, end, packedItemCode, [](const ClassificationEntry& entry, uint32_t code) { return entry.code < code; });
		if (found == end || found->code != packedItemCode) {
			return std::nullopt;
		}
		return Facts {
			.gearSlot     = found->gearSlot,
			.nonGearKind  = found->nonGearKind,
			.isGear       = found->isGear,
			.isTwoHanded  = found->isTwoHanded,
			.baseLevelReq = found->baseLevelReq,
			.width        = found->width,
			.height       = found->height,
		};
	}

	const auto* begin = std::cbegin(kTable);
	const auto* end   = std::cend(kTable);
	const auto* found = std::lower_bound(begin, end, packedItemCode, [](const ClassificationEntry& entry, uint32_t code) { return entry.code < code; });
	if (found == end || found->code != packedItemCode) {
		return std::nullopt;
	}

	return Facts {
		.gearSlot     = found->gearSlot,
		.nonGearKind  = found->nonGearKind,
		.isGear       = found->isGear,
		.isTwoHanded  = found->isTwoHanded,
		.baseLevelReq = found->baseLevelReq,
		.width        = found->width,
		.height       = found->height,
	};
}

std::string SetGroupForRow(int32_t qualityRecordId) {
	if (qualityRecordId < 0) {
		return {};
	}
	const auto index = static_cast<size_t>(qualityRecordId);

	if (g_override) {
		const auto& rows = g_override->setGroupByRow;
		return index < rows.size() ? rows[index] : std::string();
	}

	if (index >= std::size(kSetGroupByRow)) {
		return {};
	}
	const char* group = kSetGroupByRow[index];
	return group != nullptr ? std::string(group) : std::string();
}

bool LoadFromModFolder(const std::string& modDataPath, std::string* error) {
	auto loaded = ModData::LoadFromFolder(modDataPath, error);
	if (!loaded) {
		g_override.reset();
		return false;
	}
	g_override = std::move(loaded);
	return true;
}

void UseEmbeddedTable() {
	g_override.reset();
}

bool IsUsingModFolder() {
	return g_override.has_value();
}

}
