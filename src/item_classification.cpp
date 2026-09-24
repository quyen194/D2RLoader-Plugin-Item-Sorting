#include "item_classification.hpp"
#include "item_classification_data.hpp"

#include <algorithm>

namespace RemoteStashSorting::Classification {

std::optional<Facts> Classify(uint32_t packedItemCode) {
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
	if (index >= std::size(kSetGroupByRow)) {
		return {};
	}
	const char* group = kSetGroupByRow[index];
	return group != nullptr ? std::string(group) : std::string();
}

}
