// Linux/any-compiler test harness for the single-grid engine (grid_sort.*),
// its TOML reader (grid_config_toml.hpp) and the misc-item classification.
// No PluginSDK needed. Build and run from the repo root, e.g.:
//
//   g++ -std=c++20 -O1 -I src -I <dir containing toml++/toml.hpp>
//       tests/grid_sort_test.cpp src/grid_sort.cpp src/sort_engine.cpp
//       src/item_classification.cpp src/mod_item_data.cpp -o grid_sort_test
//   ./grid_sort_test [<mod .mpq folder, for the mod_data_path parity check>]
//
// or configure CMake with -DQIS_BUILD_TESTS=ON (fetches toml++ like the plugin).
// Exit code 0 = all checks passed.

#include "grid_config_toml.hpp"
#include "grid_sort.hpp"
#include "item_classification.hpp"
#include "item_misc_data.hpp"
#include "sort_engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace Quyen194ItemSorting;

static int g_failures = 0;
#define CHECK(cond, ...)                                                        \
	do {                                                                        \
		if (!(cond)) {                                                          \
			++g_failures;                                                       \
			std::printf("FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);     \
			std::printf(__VA_ARGS__);                                           \
			std::printf("\n");                                                  \
		}                                                                       \
	} while (0)

static uint32_t Code(const char* s) {
	uint32_t packed = 0;
	char     padded[4] = { ' ', ' ', ' ', ' ' };
	for (int i = 0; i < 4 && s[i]; ++i) padded[i] = s[i];
	for (int i = 0; i < 4; ++i) packed |= static_cast<uint32_t>(static_cast<uint8_t>(padded[i])) << (8 * i);
	return packed;
}

static uint64_t g_nextHandle = 1;

// Builds facts the way plugin.cpp does: Classify() for gear/charm/jewel/
// ring/amulet, ClassifyMisc() for the misc kinds and for gem/rune sizes.
static ItemFacts Item(const char* code, uint32_t quality, int x, int y) {
	ItemFacts f;
	f.handle          = g_nextHandle++;
	f.code            = Code(code);
	f.quality         = quality;
	f.sharedStashPage = 0;
	f.x               = x;
	f.y               = y;
	if (auto c = Classification::Classify(f.code)) {
		f.w = c->width; f.h = c->height; f.isGear = c->isGear; f.isTwoHanded = c->isTwoHanded;
		f.gearSlot = c->gearSlot; f.nonGearKind = c->nonGearKind; f.sortLevel = c->baseLevelReq;
	}
	if (auto m = Classification::ClassifyMisc(f.code)) {
		f.miscKind = m->kind; f.miscOrder = m->order; f.miscGrade = m->grade;
		if (!Classification::Classify(f.code)) { f.w = m->width; f.h = m->height; }
	}
	return f;
}

// Places `f` at the first free cell of a W x H occupancy (row-major scan) so
// random test inventories start out legal. Returns false if it doesn't fit.
static bool PlaceRandomly(std::vector<ItemFacts>& items, ItemFacts f, uint32_t W, uint32_t H, std::mt19937& rng) {
	std::vector<bool> used(W * H, false);
	for (const auto& it : items)
		for (uint32_t y = it.y; y < it.y + it.h; ++y)
			for (uint32_t x = it.x; x < it.x + it.w; ++x) used[y * W + x] = true;
	std::vector<std::pair<int, int>> spots;
	for (uint32_t y = 0; y + f.h <= H; ++y)
		for (uint32_t x = 0; x + f.w <= W; ++x) {
			bool ok = true;
			for (uint32_t yy = y; yy < y + f.h && ok; ++yy)
				for (uint32_t xx = x; xx < x + f.w && ok; ++xx) ok = !used[yy * W + xx];
			if (ok) spots.push_back({ static_cast<int>(x), static_cast<int>(y) });
		}
	if (spots.empty()) return false;
	auto s = spots[rng() % spots.size()];
	f.x = s.first; f.y = s.second;
	items.push_back(f);
	return true;
}

// Final positions after applying `plan` must: stay inside the grid, never
// overlap, never touch locked cells / other entries' zones with moved items,
// and leave fixed items exactly where they were.
static void CheckPlanInvariants(const std::vector<ItemFacts>& items, const Grid::Config& cfg, const Grid::Plan& plan, const char* label) {
	std::map<uint64_t, std::pair<int, int>> finalPos;
	for (const auto& it : items) finalPos[it.handle] = { it.x, it.y };
	std::set<uint64_t> moved;
	for (const auto& mv : plan.moves) {
		CHECK(moved.insert(mv.handle).second, "%s: handle moved twice", label);
		finalPos[mv.handle] = { mv.destX, mv.destY };
	}
	std::vector<uint64_t> owner(cfg.width * cfg.height, 0);
	for (const auto& it : items) {
		auto [x, y] = finalPos[it.handle];
		bool inside  = x >= 0 && y >= 0 && x + static_cast<int>(it.w) <= static_cast<int>(cfg.width) && y + static_cast<int>(it.h) <= static_cast<int>(cfg.height);
		if (!inside) {
			CHECK(moved.count(it.handle) == 0, "%s: an item was moved outside the grid", label);
			continue;
		}
		for (int yy = y; yy < y + static_cast<int>(it.h); ++yy)
			for (int xx = x; xx < x + static_cast<int>(it.w); ++xx) {
				uint64_t& o = owner[yy * cfg.width + xx];
				CHECK(o == 0, "%s: cell (%d,%d) holds two items", label, xx, yy);
				o = it.handle;
				if (moved.count(it.handle)) {
					for (const auto& r : cfg.lockedCells)
						CHECK(!(xx >= r.x && xx < r.x + r.w && yy >= r.y && yy < r.y + r.h), "%s: moved into a locked cell", label);
				}
			}
	}
}

static std::string ReadFile(const std::string& path) {
	std::ifstream f(path, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static void TestShippedConfig() {
	const std::string text = ReadFile("quyen194-item-sorting.toml");
	CHECK(!text.empty(), "run from the repo root");
	toml::table root = toml::parse(text);
	for (const char* section : { "personal_stash", "inventory", "cube" }) {
		auto parsed = Grid::ParseGridSection(root, section);
		for (auto& e : parsed.errors) std::printf("  error: %s\n", e.c_str());
		for (auto& w : parsed.warnings) std::printf("  warning: %s\n", w.c_str());
		CHECK(parsed.errors.empty() && parsed.warnings.empty(), "shipped [%s] must parse cleanly", section);
		auto problems = Grid::ValidateConfig(parsed.config);
		for (auto& p : problems) std::printf("  invalid: %s\n", p.c_str());
		CHECK(problems.empty(), "shipped [%s] must validate", section);

		// Built-in defaults (used when a section is missing) must equal the shipped file.
		const auto def = Grid::DefaultConfig(section);
		const auto& got = parsed.config;
		CHECK(def.width == got.width && def.height == got.height && def.fill == got.fill && def.categoryBreak == got.categoryBreak
				&& def.onOverflow == got.onOverflow && def.keepInPlace == got.keepInPlace && def.lockedCells.size() == got.lockedCells.size()
				&& def.layout.size() == got.layout.size(),
			"DefaultConfig(%s) must match the shipped file", section);
		for (size_t i = 0; i < def.layout.size() && i < got.layout.size(); ++i) {
			const auto& a = def.layout[i];
			const auto& b = got.layout[i];
			CHECK(a.modeIds == b.modeIds && a.anchor == b.anchor && a.hasZone == b.hasZone && a.sortKeys == b.sortKeys && a.gearGroups == b.gearGroups
					&& (!a.hasZone || (a.zone.x == b.zone.x && a.zone.y == b.zone.y && a.zone.w == b.zone.w && a.zone.h == b.zone.h)),
				"DefaultConfig(%s) layout entry %zu differs from the shipped file", section, i + 1);
		}
	}

	// Plugin-wide block holds only plugin-wide keys; the shared stash has its
	// own section (plugin.cpp's LoadConfig warns about the legacy locations).
	{
		const auto* top = root["item-sorting"].as_table();
		CHECK(top != nullptr, "[item-sorting] exists");
		const std::set<std::string> allowedTop = { "enabled", "notification_channel", "notification_seconds", "debug_log_ui_messages",
			"mod_data_path", "shared_stash", "personal_stash", "inventory", "cube" };
		for (auto&& [k, v] : *top) {
			(void)v;
			CHECK(allowedTop.count(std::string(k.str())) == 1, "unexpected top-level key '%s'", std::string(k.str()).c_str());
		}
		auto shared = root["item-sorting"]["shared_stash"];
		CHECK(shared["enabled"].value<bool>().value_or(false), "shared_stash.enabled");
		CHECK(shared["grid_width"].value<int64_t>().value_or(0) == 16 && shared["grid_height"].value<int64_t>().value_or(0) == 13, "shared grid 16x13");
		const auto* layout = shared["layout"].as_array();
		CHECK(layout != nullptr && layout->size() == 14, "shared_stash.layout has the 14 shipped entries");
		for (const char* key : { "shared_stash_tab_count", "start_tab", "blank_tabs_at_start", "compact_empty_tabs_first", "add_empty_page_when",
				 "add_page_enabled", "remove_page_enabled" }) {
			CHECK(static_cast<bool>(shared[key]), "shared_stash.%s present", key);
		}
		if (layout) {
			for (auto&& row : *layout) {
				auto id = row.as_table() ? (*row.as_table())["mode_id"].value<std::string>() : std::nullopt;
				CHECK(id && IsKnownModeId(*id), "shared_stash.layout mode_id known");
			}
		}
	}

	// A missing section falls back to defaults with a warning, not an error.
	toml::table empty = toml::parse("[item-sorting]\nenabled = true\n");
	auto parsed       = Grid::ParseGridSection(empty, "cube");
	CHECK(parsed.errors.empty() && parsed.warnings.size() == 1 && parsed.config.width == 12, "missing section -> defaults");

	// Bad values are errors, never silently replaced.
	toml::table bad = toml::parse(R"(
[item-sorting.inventory]
grid_width = 20
fill_direction = "diagonal"
locked_cells = [ { x = 1, y = 2 } ]
typo_key = 1
[[item-sorting.inventory.layout]]
mode_id = "rune"
mode_ids = ["gem"]
[[item-sorting.inventory.layout]]
mode_id = "gem"
anchor = "middle"
)");
	parsed = Grid::ParseGridSection(bad, "inventory");
	CHECK(parsed.errors.size() == 5, "expected 5 errors, got %zu", parsed.errors.size());
	CHECK(parsed.warnings.size() == 1, "expected 1 warning, got %zu", parsed.warnings.size());
}

static void TestValidation() {
	Grid::Config cfg = Grid::DefaultConfig("personal_stash");
	cfg.layout[1].modeIds = { "rune" }; // duplicate of entry 1
	cfg.layout[2].sortKeys = { "-bogus" };
	cfg.keepInPlace = { "gem" };
	Grid::Entry z;
	z.modeIds = { "potion" };
	z.hasZone = true;
	z.zone    = { 13, 0, 2, 2 }; // overlaps the rune zone
	cfg.layout.push_back(z);
	auto problems = Grid::ValidateConfig(cfg);
	CHECK(problems.size() == 3, "expected 3 problems (duplicate rune, bad key, zone overlap), got %zu", problems.size());
	for (auto& p : problems) std::printf("  (expected) %s\n", p.c_str());
	auto plan = Grid::PlanSort({}, cfg);
	CHECK(plan.status == Grid::Status::ConfigError, "config error must stop the plan");
}

static void TestMiscClassification() {
	struct Want { const char* code; MiscKind kind; uint16_t order; uint16_t grade; };
	const Want wants[] = {
		{ "r01", MiscKind::Rune, 1, 0 },   { "r33", MiscKind::Rune, 33, 0 },  { "s33", MiscKind::Rune, 33, 1 },
		{ "gcv", MiscKind::Gem, 1, 1 },    { "gpw", MiscKind::Gem, 6, 5 },    { "skz", MiscKind::Gem, 7, 5 },
		{ "1di", MiscKind::Gem, 6, 7 },    { "hp1", MiscKind::Potion, 1, 1 }, { "rvl", MiscKind::Potion, 3, 2 },
		{ "box", MiscKind::Tool, 0, 0 },   { "tbk", MiscKind::Tool, 0, 0 },   { "key", MiscKind::Tool, 0, 0 },
		{ "qey", MiscKind::Quest, 0, 0 },  { "pk1", MiscKind::Quest, 0, 0 },
	};
	for (const auto& w : wants) {
		auto m = Classification::ClassifyMisc(Code(w.code));
		CHECK(m && m->kind == w.kind && m->order == w.order && m->grade == w.grade, "ClassifyMisc(%s)", w.code);
	}
	CHECK(!Classification::ClassifyMisc(Code("cm1")), "a charm is not a misc kind");
	auto box = Classification::ClassifyMisc(Code("box"));
	CHECK(box && box->width == 2 && box->height == 2, "cube is 2x2");
}

static void TestModFolderParity(const char* modDir) {
	std::string err;
	if (!Classification::LoadFromModFolder(modDir, &err)) {
		CHECK(false, "LoadFromModFolder failed: %s", err.c_str());
		return;
	}
	size_t mismatches = 0;
	for (const auto& e : Classification::kMiscTable) {
		auto m = Classification::ClassifyMisc(e.code);
		if (!m || m->kind != e.kind || m->order != e.order || m->grade != e.grade || m->width != e.width || m->height != e.height) ++mismatches;
	}
	CHECK(mismatches == 0, "runtime misc classification differs from the generated table for %zu code(s)", mismatches);
	Classification::UseEmbeddedTable();
}

static void TestScenarios() {
	// Inventory defaults: cube goes top-right, tome next to it, loot top-left,
	// charms (keep_in_place) untouched.
	{
		Grid::Config cfg = Grid::DefaultConfig("inventory");
		std::vector<ItemFacts> items = {
			Item("box", 2, 3, 3), Item("tbk", 2, 0, 0), Item("hp5", 2, 5, 5), Item("cm3", 4, 9, 5), Item("r10", 2, 7, 7),
		};
		auto plan = Grid::PlanSort(items, cfg);
		CHECK(plan.status == Grid::Status::Ok, "inventory plan: %s", plan.error.c_str());
		std::map<uint64_t, std::pair<int, int>> pos;
		for (auto& it : items) pos[it.handle] = { it.x, it.y };
		for (auto& mv : plan.moves) pos[mv.handle] = { mv.destX, mv.destY };
		CHECK(pos[items[0].handle] == std::make_pair(8, 0), "cube at (8,0), got (%d,%d)", pos[items[0].handle].first, pos[items[0].handle].second);
		// columns fill from the right: the tome continues down column 9 under the cube
		CHECK(pos[items[1].handle] == std::make_pair(9, 2), "tome at (9,2), got (%d,%d)", pos[items[1].handle].first, pos[items[1].handle].second);
		CHECK(pos[items[3].handle] == std::make_pair(9, 5), "charm untouched");
		CHECK(plan.itemsFixed == 1, "one fixed item (the charm), got %u", plan.itemsFixed);
		CheckPlanInvariants(items, cfg, plan, "inventory");
	}
	// Personal stash: runes go to the right-most zone, El on top.
	{
		Grid::Config cfg = Grid::DefaultConfig("personal_stash");
		std::vector<ItemFacts> items = { Item("r05", 2, 0, 0), Item("r01", 2, 1, 0), Item("r33", 2, 2, 0), Item("s01", 2, 3, 0) };
		auto plan = Grid::PlanSort(items, cfg);
		CHECK(plan.status == Grid::Status::Ok, "stash plan: %s", plan.error.c_str());
		std::map<uint64_t, std::pair<int, int>> pos;
		for (auto& mv : plan.moves) pos[mv.handle] = { mv.destX, mv.destY };
		CHECK(pos[items[1].handle] == std::make_pair(15, 0), "El at (15,0)");
		CHECK(pos[items[3].handle] == std::make_pair(15, 1), "El stack right after El");
		CHECK(pos[items[0].handle] == std::make_pair(15, 2), "Eth next");
		CHECK(pos[items[2].handle] == std::make_pair(15, 3), "Zod last");
	}
	// Band: the second top-left entry starts in a fresh column.
	{
		Grid::Config cfg;
		cfg.name = "band"; cfg.width = 6; cfg.height = 4;
		cfg.layout = { Grid::Entry { .modeIds = { "rune" } }, Grid::Entry { .modeIds = { "gem" } } };
		std::vector<ItemFacts> items = { Item("r01", 2, 5, 3), Item("gcv", 2, 4, 3) };
		auto plan = Grid::PlanSort(items, cfg);
		std::map<uint64_t, std::pair<int, int>> pos;
		for (auto& mv : plan.moves) pos[mv.handle] = { mv.destX, mv.destY };
		CHECK(pos[items[0].handle] == std::make_pair(0, 0) && pos[items[1].handle] == std::make_pair(1, 0), "band puts the gem in column 1");
		cfg.categoryBreak = Grid::CategoryBreak::None;
		plan = Grid::PlanSort(items, cfg);
		pos.clear();
		for (auto& mv : plan.moves) pos[mv.handle] = { mv.destX, mv.destY };
		CHECK(pos[items[1].handle] == std::make_pair(0, 1), "none puts the gem right under the rune");
	}
	// Overflow: a 2-wide grid with bands can't hold rune + 2x2 cube; compact can.
	{
		Grid::Config cfg;
		cfg.name = "overflow"; cfg.width = 3; cfg.height = 2;
		cfg.layout = { Grid::Entry { .modeIds = { "rune" } }, Grid::Entry { .modeIds = { "tool" } } };
		std::vector<ItemFacts> items = { Item("r01", 2, 0, 0), Item("r02", 2, 0, 1), Item("box", 2, 1, 0) };
		items.push_back(Item("r03", 2, 0, 0)); // no room at all: 7 cells > 6
		auto plan = Grid::PlanSort(items, cfg);
		CHECK(plan.status == Grid::Status::NoRoom, "7 cells into 6 must fail");
		items.pop_back();
		items[0].x = 2; items[0].y = 0; items[1].x = 2; items[1].y = 1; items[2].x = 0;
		plan = Grid::PlanSort(items, cfg);
		CHECK(plan.status == Grid::Status::Ok && !plan.compactRetryUsed, "rune column + cube fits with band");
		cfg.onOverflow = Grid::Overflow::Abort;
		items.push_back(Item("r03", 2, 0, 0));
		items.back().x = 0; items.back().y = 0; items[2].x = 1;
		// 3 runes + cube = 7 > 6 again; use 2 runes + cube with a locked cell instead
		items.pop_back();
		cfg.lockedCells = { Grid::Rect { 2, 1, 1, 1 } };
		items[1].x = 2; items[1].y = 0; items[0].x = 0; items[0].y = 0; items[2].x = 0; items[2].y = 0;
		items[0].x = 2; items[0].y = 0; items[1].x = 1; items[1].y = 1; // r01 (2,0), r02 (1,1), cube must go (0,0) 2x2 -> overlaps r02
		items[2].x = 0; items[2].y = 0; items[1].x = 0; items[1].y = 0; // keep inputs legal: rebuild below
		std::vector<ItemFacts> legal = { Item("r01", 2, 2, 0), Item("r02", 2, 0, 0), Item("box", 2, 0, 0) };
		legal[2].x = 0; legal[2].y = 0; legal[1].x = 2; legal[1].y = 0; legal[0].x = 2; legal[0].y = 1; // r02 (2,0), r01 (2,1) locked, cube (0,0)
		plan = Grid::PlanSort(legal, cfg);
		// r01 overlaps the locked cell -> fixed; r02 goes to column 0 (band) -> cube needs a 2x2 in columns 1-2 minus locked -> no room with
		// band; compact isn't allowed (abort) -> NoRoom.
		CHECK(plan.status == Grid::Status::NoRoom && !plan.compactRetryUsed, "abort must not retry");
		cfg.onOverflow = Grid::Overflow::Compact;
		plan = Grid::PlanSort(legal, cfg);
		CHECK(plan.compactRetryUsed, "compact must retry");
	}
	// Items outside the configured grid are ignored, not errors.
	{
		Grid::Config cfg = Grid::DefaultConfig("cube");
		cfg.width = 3; cfg.height = 4;
		std::vector<ItemFacts> items = { Item("r01", 2, 5, 5), Item("r02", 2, 2, 3) };
		auto plan = Grid::PlanSort(items, cfg);
		CHECK(plan.status == Grid::Status::Ok && plan.itemsOutsideGrid == 1 && plan.moves.size() == 1, "outside item ignored");
	}
}

static void TestRandomized() {
	std::mt19937 rng(12345);
	const char*  codes[] = { "r01", "r02", "r15", "r33", "s05", "gcv", "gpw", "skz", "1am", "hp3", "mp5", "rvl", "box", "tbk", "ibk", "key",
		 "qey", "pk1", "bks", "cm1", "cm2", "cm3", "jew", "rin", "amu", "cap", "lea", "hgl", "lbt", "buc", "hax", "lsd", "ssd", "bst", "gix",
		 "stf", "lbw", "xbw", "aqv", "cqv", "elx", "oos" };
	const uint32_t qualities[] = { 2, 3, 4, 5, 6, 7, 8 };
	int plans = 0, noRoom = 0;
	for (const char* section : { "personal_stash", "inventory", "cube" }) {
		for (int round = 0; round < 400; ++round) {
			Grid::Config cfg = Grid::DefaultConfig(section);
			if (round % 3 == 1) cfg.lockedCells = { Grid::Rect { 0, 0, 1, 2 } };
			if (round % 5 == 2) cfg.fill = Grid::FillDirection::Rows;
			if (round % 7 == 3) cfg.categoryBreak = Grid::CategoryBreak::Band;
			std::vector<ItemFacts> items;
			const int target = 1 + static_cast<int>(rng() % (cfg.width * cfg.height / 2));
			for (int i = 0; i < target; ++i) {
				ItemFacts f = Item(codes[rng() % std::size(codes)], qualities[rng() % std::size(qualities)], 0, 0);
				if (f.nonGearKind == NonGearKind::Charm && f.quality != 7) f.quality = 4;
				PlaceRandomly(items, f, cfg.width, cfg.height, rng);
			}
			auto plan = Grid::PlanSort(items, cfg);
			++plans;
			if (plan.status == Grid::Status::NoRoom) {
				++noRoom;
				continue;
			}
			CHECK(plan.status == Grid::Status::Ok, "%s round %d: %s", section, round, plan.error.c_str());
			CheckPlanInvariants(items, cfg, plan, section);
			// Sorting an already-sorted grid must be a no-op.
			std::vector<ItemFacts> after = items;
			for (auto& it : after)
				for (auto& mv : plan.moves)
					if (mv.handle == it.handle) { it.x = mv.destX; it.y = mv.destY; }
			auto again = Grid::PlanSort(after, cfg);
			CHECK(again.status == Grid::Status::Ok && again.moves.empty(), "%s round %d: re-sorting moved %zu item(s)", section, round, again.moves.size());
		}
	}
	std::printf("randomized: %d plans, %d NoRoom (expected for dense random grids)\n", plans, noRoom);
}

int main(int argc, char** argv) {
	TestShippedConfig();
	TestValidation();
	TestMiscClassification();
	if (argc > 1) TestModFolderParity(argv[1]);
	TestScenarios();
	TestRandomized();
	if (g_failures == 0) std::printf("ALL PASSED\n");
	else std::printf("%d FAILURE(S)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
