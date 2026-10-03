#include "item_classification.hpp"
#include "item_classification_data.hpp"
#include "item_misc_data.hpp"
#include "mod_item_data.hpp"

#include <algorithm>

namespace ItemSorting::Classification {

namespace {
// Set by LoadFromModFolder/UseEmbeddedTable. std::nullopt means "answer from
// the embedded kTable/kSetGroupByRow below" -- the original, always-present
// behavior. This is plain (non-atomic) state: like the rest of this plugin's
// config, it is only ever written from the game thread during config
// (re)load, never read concurrently with a write.
std::optional<ModData::LoadedTable> g_override;

// D2RL::Items::Quality's numeric values, per ItemFacts' own doc comment
// (Magic=4, Set=5, Rare=6, Unique=7, Crafted=8). Not worth pulling in the SDK
// header just for these two.
constexpr uint32_t kQualitySet    = 5;
constexpr uint32_t kQualityUnique = 7;

uint32_t LevelReqAt(const uint16_t* data, size_t n, int32_t index) {
	if (index < 0 || static_cast<size_t>(index) >= n) return 0;
	return data[index];
}

uint32_t SetLevelReqForRow(int32_t qualityRecordId) {
	if (g_override) return LevelReqAt(g_override->setLevelReqByRow.data(), g_override->setLevelReqByRow.size(), qualityRecordId);
	return LevelReqAt(kSetLevelReqByRow, std::size(kSetLevelReqByRow), qualityRecordId);
}

uint32_t UniqueLevelReqForRow(int32_t qualityRecordId) {
	if (g_override) return LevelReqAt(g_override->uniqueLevelReqByRow.data(), g_override->uniqueLevelReqByRow.size(), qualityRecordId);
	return LevelReqAt(kUniqueLevelReqByRow, std::size(kUniqueLevelReqByRow), qualityRecordId);
}

// combinedId is ItemInfo::prefixIds/suffixIds' *combined* MagicSuffix+
// MagicPrefix+AutoMagic ID space (one-based; 0 means "no affix in this
// slot"): [1, numSuffixRows] is a MagicSuffix.txt row, (numSuffixRows,
// numSuffixRows+numPrefixRows] is a MagicPrefix.txt row, and anything past
// that is AutoMagic -- which resolves to 0 here, matching the save-editor's
// _ssAffixLevelReq (it never consults AutoMagic.txt for this).
uint32_t AffixLevelReq(uint32_t combinedId) {
	if (combinedId == 0) return 0;

	const uint16_t* suffixData;
	size_t          suffixN;
	const uint16_t* prefixData;
	size_t          prefixN;
	if (g_override) {
		suffixData = g_override->magicSuffixLevelReq.data();
		suffixN    = g_override->magicSuffixLevelReq.size();
		prefixData = g_override->magicPrefixLevelReq.data();
		prefixN    = g_override->magicPrefixLevelReq.size();
	} else {
		suffixData = kMagicSuffixLevelReq;
		suffixN    = std::size(kMagicSuffixLevelReq);
		prefixData = kMagicPrefixLevelReq;
		prefixN    = std::size(kMagicPrefixLevelReq);
	}

	if (combinedId <= suffixN) return suffixData[combinedId - 1];
	if (combinedId <= suffixN + prefixN) return prefixData[combinedId - suffixN - 1];
	return 0; // AutoMagic, or an ID past both tables
}
}

namespace {
// The generated table and the mod_data_path parser both call anything that
// is not a charm/jewel/ring/amulet/gem "gear", matching the save editor's
// _ssIsGearItem -- including the mod's crafting materials, keys and orbs,
// which match none of the gear slots. Those are not equipment, so they go to
// the "unclassified" catch-all instead of the gear categories.
bool IsRealGear(const ClassificationEntry& entry) {
	return entry.isGear && entry.gearSlot != GearSlot::None;
}
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
			.isGear       = IsRealGear(*found),
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
		.isGear       = IsRealGear(*found),
		.isTwoHanded  = found->isTwoHanded,
		.baseLevelReq = found->baseLevelReq,
		.width        = found->width,
		.height       = found->height,
	};
}

std::optional<MiscFacts> ClassifyMisc(uint32_t packedItemCode) {
	const MiscEntry* begin;
	const MiscEntry* end;
	if (g_override) {
		begin = g_override->miscEntries.data();
		end   = begin + g_override->miscEntries.size();
	} else {
		begin = std::cbegin(kMiscTable);
		end   = std::cend(kMiscTable);
	}
	const auto* found = std::lower_bound(begin, end, packedItemCode, [](const MiscEntry& entry, uint32_t code) { return entry.code < code; });
	if (found == end || found->code != packedItemCode) {
		return std::nullopt;
	}
	return MiscFacts { .kind = found->kind, .order = found->order, .grade = found->grade, .width = found->width, .height = found->height };
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

uint32_t TrueReqLevel(
    uint32_t quality, int32_t qualityRecordId, const uint32_t (&prefixIds)[3], const uint32_t (&suffixIds)[3], uint32_t baseLevelReq) {
	uint32_t lvl = baseLevelReq;

	if (quality == kQualityUnique) {
		lvl = (std::max)(lvl, UniqueLevelReqForRow(qualityRecordId));
	} else if (quality == kQualitySet) {
		lvl = (std::max)(lvl, SetLevelReqForRow(qualityRecordId));
	}

	for (uint32_t id : prefixIds) lvl = (std::max)(lvl, AffixLevelReq(id));
	for (uint32_t id : suffixIds) lvl = (std::max)(lvl, AffixLevelReq(id));

	return lvl;
}

}
