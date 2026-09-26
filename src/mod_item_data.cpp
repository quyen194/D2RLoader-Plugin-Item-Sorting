#include "mod_item_data.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace RemoteStashSorting::Classification::ModData {

namespace {

// ---- tiny tab-separated table reader (mirrors gen_classification.py's
// read_table: header row names every column, later rows are looked up by
// name, a short row is padded with "" for missing trailing cells) ----------

struct Table {
	std::vector<std::string>              header;
	std::unordered_map<std::string, size_t> colIndex;
	std::vector<std::vector<std::string>> rows;

	// Returns "" if the row doesn't have this column (short row) or the
	// column name is unknown -- same leniency as the Python version.
	const std::string& Cell(const std::vector<std::string>& row, const std::string& col) const {
		static const std::string kEmpty;
		auto it = colIndex.find(col);
		if (it == colIndex.end() || it->second >= row.size()) return kEmpty;
		return row[it->second];
	}
};

std::vector<std::string> SplitTab(const std::string& line) {
	std::vector<std::string> out;
	size_t                   start = 0;
	while (true) {
		size_t tab = line.find('\t', start);
		if (tab == std::string::npos) {
			out.push_back(line.substr(start));
			break;
		}
		out.push_back(line.substr(start, tab - start));
		start = tab + 1;
	}
	return out;
}

std::string StripCr(std::string s) {
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
	return s;
}

// Reads and tab-splits every non-empty line of `path`. Returns false (with
// *error set) if the file can't be opened or has no header line.
bool ReadTable(const std::string& path, Table* out, std::string* error) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		if (error != nullptr) *error = "could not open '" + path + "'";
		return false;
	}

	std::vector<std::string> lines;
	std::string              raw;
	while (std::getline(file, raw)) {
		std::string line = StripCr(raw);
		if (!line.empty()) lines.push_back(std::move(line));
	}
	if (lines.empty()) {
		if (error != nullptr) *error = "'" + path + "' is empty (no header row)";
		return false;
	}

	out->header = SplitTab(lines[0]);
	out->colIndex.clear();
	for (size_t i = 0; i < out->header.size(); ++i) out->colIndex.emplace(out->header[i], i);

	out->rows.clear();
	out->rows.reserve(lines.size() - 1);
	for (size_t i = 1; i < lines.size(); ++i) out->rows.push_back(SplitTab(lines[i]));
	return true;
}

// ---- buildItemTypeChains (itemtypes.txt Equiv1/Equiv2 walk) --------------

using EquivMap = std::unordered_map<std::string, std::vector<std::string>>;

// Mirrors gen_classification.py's chain(): a DFS over Equiv1/Equiv2 with a
// visited set to guard against cycles in the data.
std::unordered_set<std::string> Chain(const std::string& code, const EquivMap& equiv, std::unordered_set<std::string>* visited) {
	if (visited->count(code) != 0) return {};
	visited->insert(code);
	std::unordered_set<std::string> out { code };
	auto                            found = equiv.find(code);
	if (found != equiv.end()) {
		for (const auto& eq : found->second) {
			auto sub = Chain(eq, equiv, visited);
			out.insert(sub.begin(), sub.end());
		}
	}
	return out;
}

// ---- SS_GEAR_GROUPS: order matches the GearSlot enum (None=0, then 1..23) --

struct GearGroupDef {
	GearSlot                 slot;
	std::vector<std::string> types;
};

const std::vector<GearGroupDef>& GearGroups() {
	static const std::vector<GearGroupDef> groups = {
		{ GearSlot::Helm, { "helm" } },
		{ GearSlot::Torso, { "tors" } },
		{ GearSlot::Belt, { "belt" } },
		{ GearSlot::Gloves, { "glov" } },
		{ GearSlot::Boots, { "boot" } },
		{ GearSlot::Shield, { "shld" } },
		{ GearSlot::Axe, { "axe" } },
		{ GearSlot::Sword, { "swor" } },
		{ GearSlot::Knife, { "knif" } },
		{ GearSlot::ClubMace, { "club", "mace", "hamm" } },
		{ GearSlot::Scepter, { "scep" } },
		{ GearSlot::Wand, { "wand" } },
		{ GearSlot::Staff, { "staf" } },
		{ GearSlot::Orb, { "orb" } },
		{ GearSlot::Spear, { "spea" } },
		{ GearSlot::Polearm, { "pole" } },
		{ GearSlot::Javelin, { "jave" } },
		{ GearSlot::Bow, { "bow" } },
		{ GearSlot::Arrows, { "bowq" } },
		{ GearSlot::Crossbow, { "xbow" } },
		{ GearSlot::Bolts, { "xboq" } },
		{ GearSlot::Claw, { "h2h" } },
		{ GearSlot::OtherWeapon, { "weap" } },
	};
	return groups;
}

int ParseIntOr(const std::string& s, int fallback) {
	if (s.empty()) return fallback;
	try {
		return std::stoi(s);
	} catch (...) {
		return fallback;
	}
}

uint32_t MakeItemCodeLE(const std::string& code) {
	std::string padded = code;
	padded.resize(4, ' ');
	uint32_t packed = 0;
	for (int i = 0; i < 4; ++i) packed |= (static_cast<uint32_t>(static_cast<uint8_t>(padded[i])) << (8 * i));
	return packed;
}

}

std::optional<LoadedTable> LoadFromFolder(const std::string& modDataPath, std::string* error) {
	auto fail = [&](const std::string& msg) -> std::optional<LoadedTable> {
		if (error != nullptr) *error = msg;
		return std::nullopt;
	};

	if (modDataPath.empty()) {
		return fail("modDataPath is empty");
	}

	const char sep = '/';
	// Accept a path with either slash style; normalize to '/' before joining.
	std::string base = modDataPath;
	std::replace(base.begin(), base.end(), '\\', '/');
	while (!base.empty() && base.back() == '/') base.pop_back();
	const std::string excelDir = base + "/data/global/excel/";

	Table itemtypesT, weaponsT, armorT, miscT, setitemsT;
	std::string err;
	if (!ReadTable(excelDir + "itemtypes.txt", &itemtypesT, &err)) return fail(err);
	if (!ReadTable(excelDir + "weapons.txt", &weaponsT, &err)) return fail(err);
	if (!ReadTable(excelDir + "armor.txt", &armorT, &err)) return fail(err);
	if (!ReadTable(excelDir + "misc.txt", &miscT, &err)) return fail(err);
	if (!ReadTable(excelDir + "setitems.txt", &setitemsT, &err)) return fail(err);

	// ---- buildItemTypeChains ----
	EquivMap equiv;
	for (const auto& row : itemtypesT.rows) {
		std::string code = itemtypesT.Cell(row, "Code");
		if (code.empty()) continue;
		std::vector<std::string> eqs;
		std::string              eq1 = itemtypesT.Cell(row, "Equiv1");
		std::string              eq2 = itemtypesT.Cell(row, "Equiv2");
		if (!eq1.empty()) eqs.push_back(eq1);
		if (!eq2.empty()) eqs.push_back(eq2);
		equiv.emplace(code, std::move(eqs));
	}
	auto typeChainHas = [&equiv](const std::string& baseType, const std::vector<std::string>& wanted) -> bool {
		if (baseType.empty()) return false;
		std::unordered_set<std::string> visited;
		std::unordered_set<std::string> ch;
		if (equiv.count(baseType) != 0) {
			ch = Chain(baseType, equiv, &visited);
		} else {
			ch = { baseType };
		}
		for (const auto& w : wanted) {
			if (ch.count(w) != 0) return true;
		}
		return false;
	};

	// ---- buildSocketAndTypeMap + weapon/belt meta ----
	std::unordered_map<std::string, std::string> itemTypeByCode;
	std::unordered_map<std::string, int>         levelReqByCode;
	std::unordered_map<std::string, std::pair<int, int>> sizeByCode;
	std::unordered_set<std::string>              is2Handed;

	for (const Table* t : { &weaponsT, &armorT, &miscT }) {
		bool isWeapons = (t == &weaponsT);
		for (const auto& row : t->rows) {
			std::string code = t->Cell(row, "code");
			if (code.empty()) continue;
			std::string type = t->Cell(row, "type");
			if (!type.empty()) itemTypeByCode[code] = type;
			levelReqByCode[code] = ParseIntOr(t->Cell(row, "levelreq"), 0);
			int w = std::max(1, ParseIntOr(t->Cell(row, "invwidth"), 1));
			int h = std::max(1, ParseIntOr(t->Cell(row, "invheight"), 1));
			sizeByCode[code] = { w, h };
			if (isWeapons && t->Cell(row, "2handed") == "1") is2Handed.insert(code);
		}
	}

	// ---- classify() ----
	struct ClassifyResult {
		GearSlot    gearSlot;
		NonGearKind nonGearKind;
		bool        isGear;
		bool        isTwoHanded;
	};
	auto classify = [&](const std::string& code) -> std::optional<ClassifyResult> {
		auto btIt = itemTypeByCode.find(code);
		if (btIt == itemTypeByCode.end() || btIt->second.empty()) return std::nullopt;
		const std::string& baseType = btIt->second;

		if (typeChainHas(baseType, { "char", "torc" })) return ClassifyResult { GearSlot::None, NonGearKind::Charm, false, false };
		if (typeChainHas(baseType, { "jewl" })) return ClassifyResult { GearSlot::None, NonGearKind::Jewel, false, false };
		if (typeChainHas(baseType, { "ring" })) return ClassifyResult { GearSlot::None, NonGearKind::Ring, false, false };
		if (typeChainHas(baseType, { "amul" })) return ClassifyResult { GearSlot::None, NonGearKind::Amulet, false, false };
		if (typeChainHas(baseType, { "gem", "sock" })) return std::nullopt;

		GearSlot gearSlot = GearSlot::None;
		for (const auto& group : GearGroups()) {
			if (typeChainHas(baseType, group.types)) {
				gearSlot = group.slot;
				break;
			}
		}
		bool isTwoHanded = is2Handed.count(code) != 0;
		// NOTE: isGear is unconditionally true here, matching
		// gen_classification.py exactly (and the save-editor's own
		// _ssIsGearItem, which this ports) -- a code that isn't a
		// charm/jewel/ring/amulet/gem/socket-filler counts as "gear" even
		// when it also matches none of the 23 real gear slots (GearSlot::
		// None). See README "Known gaps" for the class of item this
		// produces (Reimagined's own crafting orbs/materials, keys, etc).
		return ClassifyResult { gearSlot, NonGearKind::None, true, isTwoHanded };
	};

	LoadedTable result;
	result.entries.reserve(itemTypeByCode.size());
	for (const auto& [code, baseType] : itemTypeByCode) {
		(void)baseType;
		auto cr = classify(code);
		if (!cr) continue;
		ClassificationEntry entry {};
		entry.code         = MakeItemCodeLE(code);
		entry.gearSlot      = cr->gearSlot;
		entry.nonGearKind   = cr->nonGearKind;
		entry.isGear        = cr->isGear;
		entry.isTwoHanded   = cr->isTwoHanded;
		entry.baseLevelReq  = static_cast<uint16_t>(std::clamp(levelReqByCode.count(code) ? levelReqByCode[code] : 0, 0, 65535));
		auto sizeIt         = sizeByCode.find(code);
		entry.width         = static_cast<uint8_t>(sizeIt != sizeByCode.end() ? std::clamp(sizeIt->second.first, 1, 255) : 1);
		entry.height        = static_cast<uint8_t>(sizeIt != sizeByCode.end() ? std::clamp(sizeIt->second.second, 1, 255) : 1);
		result.entries.push_back(entry);
	}
	std::sort(result.entries.begin(), result.entries.end(), [](const ClassificationEntry& a, const ClassificationEntry& b) { return a.code < b.code; });
	for (size_t i = 1; i < result.entries.size(); ++i) {
		if (result.entries[i].code == result.entries[i - 1].code) {
			return fail("duplicate packed item code while building the classification table (mod data may be malformed)");
		}
	}

	// ---- setitems.txt: row position (skipping genuinely-unused rows) ->
	// raw 'set' column -- mirrors the (now-fixed) gen_classification.py.
	result.setGroupByRow.reserve(setitemsT.rows.size());
	for (const auto& row : setitemsT.rows) {
		std::string item  = setitemsT.Cell(row, "item");
		std::string setV  = setitemsT.Cell(row, "set");
		if (item.empty() && setV.empty()) continue; // unused row -- no live qualityRecordId slot
		result.setGroupByRow.push_back(setV);
	}

	return result;
}

}
