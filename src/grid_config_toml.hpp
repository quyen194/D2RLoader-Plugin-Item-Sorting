#pragma once

// TOML -> Grid::Config for the [quyen194-item-sorting.personal_stash] /
// .inventory / .cube sections. Header-only and free of D2RLPlugin so the
// Linux test harness (tests/grid_sort_test.cpp) can run it against the real
// shipped config file.
//
// Semantics:
//   - section missing entirely: Grid::DefaultConfig(section), plus a note in
//     *warnings (an older config file predates these sections);
//   - key missing: that key's value from Grid::DefaultConfig(section) (the
//     layout list included);
//   - key present with a wrong type/value: an entry in *errors. The caller
//     refuses to sort that container while *errors is non-empty -- a typo
//     must never silently turn into "sort with some other setting";
//   - unknown key: an entry in *warnings (probably a typo), otherwise ignored.
// Grid::ValidateConfig then checks the cross-field rules (zones inside the
// grid, no duplicate modes, ...).

#include "grid_sort.hpp"

#include <toml++/toml.hpp>

#include <string>
#include <unordered_set>
#include <vector>

namespace Quyen194ItemSorting::Grid {

struct ParsedSection {
	bool                     enabled = true;
	Config                   config;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

namespace TomlDetail {

inline bool ReadRect(const toml::node& node, Rect* out) {
	const auto* t = node.as_table();
	if (t == nullptr) return false;
	for (auto&& [k, v] : *t) {
		const std::string key(k.str());
		if (key != "x" && key != "y" && key != "w" && key != "h") return false;
		if (!v.is_integer()) return false;
	}
	auto x = (*t)["x"].value<int64_t>();
	auto y = (*t)["y"].value<int64_t>();
	auto w = (*t)["w"].value<int64_t>();
	auto h = (*t)["h"].value<int64_t>();
	if (!x || !y || !w || !h) return false;
	*out = Rect { static_cast<int32_t>(*x), static_cast<int32_t>(*y), static_cast<int32_t>(*w), static_cast<int32_t>(*h) };
	return true;
}

inline bool ReadStringArray(const toml::node& node, std::vector<std::string>* out) {
	const auto* arr = node.as_array();
	if (arr == nullptr) return false;
	std::vector<std::string> values;
	for (auto&& v : *arr) {
		auto s = v.value<std::string>();
		if (!s) return false;
		values.push_back(std::move(*s));
	}
	*out = std::move(values);
	return true;
}

} // namespace TomlDetail

// `root` is the whole parsed file; `section` is "personal_stash",
// "inventory" or "cube".
inline ParsedSection ParseGridSection(const toml::table& root, const std::string& section) {
	using namespace TomlDetail;
	ParsedSection out;
	out.config            = DefaultConfig(section);
	const std::string pfx = "[quyen194-item-sorting." + section + "] ";

	const toml::table* plugin = root["quyen194-item-sorting"].as_table();
	const toml::table* t      = plugin != nullptr ? (*plugin)[section].as_table() : nullptr;
	if (t == nullptr) {
		out.warnings.push_back(pfx + "section not found in the config file; using the built-in defaults");
		return out;
	}

	static const std::unordered_set<std::string> kKnown = {
		"enabled", "grid_width", "grid_height", "fill_direction", "category_break", "on_overflow", "locked_cells", "keep_in_place", "layout",
	};
	for (auto&& [k, v] : *t) {
		(void)v;
		const std::string key(k.str());
		if (kKnown.count(key) == 0) out.warnings.push_back(pfx + "unknown key \"" + key + "\" ignored");
	}

	Config& cfg = out.config;
	auto    err = [&](const std::string& msg) { out.errors.push_back(pfx + msg); };

	if (const auto* n = t->get("enabled")) {
		if (auto b = n->value<bool>(); b && n->is_boolean()) out.enabled = *b;
		else err("enabled must be true or false");
	}
	auto readSide = [&](const char* key, uint32_t* dst) {
		if (const auto* n = t->get(key)) {
			auto v = n->value<int64_t>();
			if (n->is_integer() && v && *v >= 1 && *v <= static_cast<int64_t>(kMaxGridSide)) *dst = static_cast<uint32_t>(*v);
			else err(std::string(key) + " must be an integer 1-16");
		}
	};
	readSide("grid_width", &cfg.width);
	readSide("grid_height", &cfg.height);

	auto readEnum = [&](const char* key, auto parse, auto* dst, const char* allowed) {
		if (const auto* n = t->get(key)) {
			auto s = n->value<std::string>();
			if (!(s && parse(*s, dst))) err(std::string(key) + " must be " + allowed);
		}
	};
	readEnum("fill_direction", ParseFillDirection, &cfg.fill, "\"columns\" or \"rows\"");
	readEnum("category_break", ParseCategoryBreak, &cfg.categoryBreak, "\"none\" or \"band\"");
	readEnum("on_overflow", ParseOverflow, &cfg.onOverflow, "\"compact\" or \"abort\"");

	if (const auto* n = t->get("locked_cells")) {
		cfg.lockedCells.clear();
		const auto* arr = n->as_array();
		if (arr == nullptr) {
			err("locked_cells must be a list of { x, y, w, h } tables");
		} else {
			for (auto&& v : *arr) {
				Rect r;
				if (ReadRect(v, &r)) cfg.lockedCells.push_back(r);
				else err("locked_cells: every entry must be { x = <int>, y = <int>, w = <int>, h = <int> }");
			}
		}
	}
	if (const auto* n = t->get("keep_in_place")) {
		if (!ReadStringArray(*n, &cfg.keepInPlace)) err("keep_in_place must be a list of mode_id strings");
	}

	if (const auto* n = t->get("layout")) {
		const auto* arr = n->as_array();
		if (arr == nullptr) {
			err("layout must be written as [[quyen194-item-sorting." + section + ".layout]] entries");
		} else {
			cfg.layout.clear();
			size_t index = 0;
			for (auto&& v : *arr) {
				++index;
				const std::string ep  = "layout entry " + std::to_string(index) + ": ";
				const auto*       row = v.as_table();
				if (row == nullptr) {
					err(ep + "not a table");
					continue;
				}
				static const std::unordered_set<std::string> kEntryKeys = { "mode_id", "mode_ids", "anchor", "zone", "sort_keys", "gear_groups" };
				for (auto&& [k, vv] : *row) {
					(void)vv;
					const std::string key(k.str());
					if (kEntryKeys.count(key) == 0) out.warnings.push_back(pfx + ep + "unknown key \"" + key + "\" ignored");
				}

				Entry       e;
				const auto* modeId  = row->get("mode_id");
				const auto* modeIds = row->get("mode_ids");
				if ((modeId != nullptr) == (modeIds != nullptr)) {
					err(ep + "set exactly one of mode_id or mode_ids");
				} else if (modeId != nullptr) {
					auto s = modeId->value<std::string>();
					if (s && modeId->is_string()) e.modeIds = { *s };
					else err(ep + "mode_id must be a string");
				} else if (!ReadStringArray(*modeIds, &e.modeIds) || e.modeIds.empty()) {
					err(ep + "mode_ids must be a non-empty list of strings");
				}
				if (const auto* a = row->get("anchor")) {
					auto s = a->value<std::string>();
					if (!(s && ParseAnchor(*s, &e.anchor))) err(ep + "anchor must be \"topLeft\", \"topRight\", \"bottomLeft\" or \"bottomRight\"");
				}
				if (const auto* z = row->get("zone")) {
					if (ReadRect(*z, &e.zone)) e.hasZone = true;
					else err(ep + "zone must be { x = <int>, y = <int>, w = <int>, h = <int> }");
				}
				if (const auto* k = row->get("sort_keys")) {
					if (!ReadStringArray(*k, &e.sortKeys)) err(ep + "sort_keys must be a list of strings");
				}
				if (const auto* g = row->get("gear_groups")) {
					if (!ReadStringArray(*g, &e.gearGroups)) err(ep + "gear_groups must be a list of strings");
				}
				cfg.layout.push_back(std::move(e));
			}
		}
	}
	return out;
}

}
