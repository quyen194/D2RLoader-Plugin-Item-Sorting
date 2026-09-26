#pragma once

// GENERATED FILE -- do not edit by hand.
// Produced by gen_classification.py from D2R-Reimagined-Save-Editor's
// base/{itemtypes,weapons,armor,misc}.txt (see that script for the exact
// port of buildItemTypeChains/buildSocketAndTypeMap/SS_GEAR_GROUPS).
//
// 724 classified item codes.

#include "sort_engine.hpp"
#include <cstdint>

namespace RemoteStashSorting::Classification {

struct ClassificationEntry {
	uint32_t    code; // packed 4-byte item code, ascending, for binary search
	GearSlot    gearSlot;
	NonGearKind nonGearKind;
	bool        isGear;
	bool        isTwoHanded;
	uint16_t    baseLevelReq;
	uint8_t     width;  // inventory footprint, from *.txt invwidth
	uint8_t     height; // inventory footprint, from *.txt invheight
};

inline constexpr ClassificationEntry kTable[724] = {
	{ 0x20303173U, GearSlot::None, NonGearKind::None, true, false, 23u, 1u, 1u }, // s10
	{ 0x20303273U, GearSlot::None, NonGearKind::None, true, false, 43u, 1u, 1u }, // s20
	{ 0x20303373U, GearSlot::None, NonGearKind::None, true, false, 63u, 1u, 1u }, // s30
	{ 0x20306963U, GearSlot::Helm, NonGearKind::None, true, false, 16u, 2u, 2u }, // ci0
	{ 0x20313073U, GearSlot::None, NonGearKind::None, true, false, 11u, 1u, 1u }, // s01
	{ 0x2031307AU, GearSlot::Arrows, NonGearKind::None, true, false, 1u, 1u, 3u }, // z01
	{ 0x20313173U, GearSlot::None, NonGearKind::None, true, false, 25u, 1u, 1u }, // s11
	{ 0x20313273U, GearSlot::None, NonGearKind::None, true, false, 45u, 1u, 1u }, // s21
	{ 0x20313373U, GearSlot::None, NonGearKind::None, true, false, 65u, 1u, 1u }, // s31
	{ 0x20316162U, GearSlot::Helm, NonGearKind::None, true, false, 3u, 2u, 2u }, // ba1
	{ 0x20316170U, GearSlot::Shield, NonGearKind::None, true, false, 3u, 2u, 2u }, // pa1
	{ 0x20316175U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // ua1
	{ 0x20316177U, GearSlot::Shield, NonGearKind::None, true, false, 3u, 2u, 2u }, // wa1
	{ 0x20316178U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xa1
	{ 0x2031626FU, GearSlot::Orb, NonGearKind::None, true, false, 0u, 1u, 2u }, // ob1
	{ 0x2031656EU, GearSlot::Shield, NonGearKind::None, true, false, 3u, 2u, 2u }, // ne1
	{ 0x20316671U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 2u, 3u }, // qf1
	{ 0x20316963U, GearSlot::Helm, NonGearKind::None, true, false, 39u, 2u, 2u }, // ci1
	{ 0x20316B70U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // pk1
	{ 0x20316D61U, GearSlot::Bow, NonGearKind::None, true, true, 14u, 2u, 4u }, // am1
	{ 0x20316D63U, GearSlot::None, NonGearKind::Charm, false, false, 0u, 1u, 1u }, // cm1
	{ 0x20316D75U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // um1
	{ 0x20317068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hp1
	{ 0x2031706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mp1
	{ 0x20317264U, GearSlot::Helm, NonGearKind::None, true, false, 3u, 2u, 2u }, // dr1
	{ 0x20317274U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // tr1
	{ 0x20317363U, GearSlot::None, NonGearKind::Charm, false, false, 75u, 1u, 3u }, // cs1
	{ 0x20323073U, GearSlot::None, NonGearKind::None, true, false, 11u, 1u, 1u }, // s02
	{ 0x2032307AU, GearSlot::Bolts, NonGearKind::None, true, false, 1u, 1u, 3u }, // z02
	{ 0x20323173U, GearSlot::None, NonGearKind::None, true, false, 27u, 1u, 1u }, // s12
	{ 0x20323273U, GearSlot::None, NonGearKind::None, true, false, 47u, 1u, 1u }, // s22
	{ 0x20323373U, GearSlot::None, NonGearKind::None, true, false, 67u, 1u, 1u }, // s32
	{ 0x20326162U, GearSlot::Helm, NonGearKind::None, true, false, 6u, 2u, 2u }, // ba2
	{ 0x20326170U, GearSlot::Shield, NonGearKind::None, true, false, 6u, 2u, 2u }, // pa2
	{ 0x20326175U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // ua2
	{ 0x20326177U, GearSlot::Shield, NonGearKind::None, true, false, 6u, 2u, 2u }, // wa2
	{ 0x20326178U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xa2
	{ 0x2032626FU, GearSlot::Orb, NonGearKind::None, true, false, 0u, 1u, 2u }, // ob2
	{ 0x2032656EU, GearSlot::Shield, NonGearKind::None, true, false, 6u, 2u, 2u }, // ne2
	{ 0x20326671U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 2u, 3u }, // qf2
	{ 0x20326963U, GearSlot::Helm, NonGearKind::None, true, false, 52u, 2u, 2u }, // ci2
	{ 0x20326B70U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // pk2
	{ 0x20326D61U, GearSlot::Bow, NonGearKind::None, true, true, 20u, 2u, 4u }, // am2
	{ 0x20326D63U, GearSlot::None, NonGearKind::Charm, false, false, 0u, 1u, 2u }, // cm2
	{ 0x20326D75U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // um2
	{ 0x20327068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hp2
	{ 0x2032706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mp2
	{ 0x20327264U, GearSlot::Helm, NonGearKind::None, true, false, 6u, 2u, 2u }, // dr2
	{ 0x20327274U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // tr2
	{ 0x20327363U, GearSlot::None, NonGearKind::Charm, false, false, 75u, 1u, 3u }, // cs2
	{ 0x20333073U, GearSlot::None, NonGearKind::None, true, false, 13u, 1u, 1u }, // s03
	{ 0x20333173U, GearSlot::None, NonGearKind::None, true, false, 29u, 1u, 1u }, // s13
	{ 0x20333273U, GearSlot::None, NonGearKind::None, true, false, 49u, 1u, 1u }, // s23
	{ 0x20333364U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 2u }, // d33
	{ 0x20333367U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 2u }, // g33
	{ 0x20333373U, GearSlot::None, NonGearKind::None, true, false, 69u, 1u, 1u }, // s33
	{ 0x20336162U, GearSlot::Helm, NonGearKind::None, true, false, 12u, 2u, 2u }, // ba3
	{ 0x2033616BU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ka3
	{ 0x20336170U, GearSlot::Shield, NonGearKind::None, true, false, 12u, 2u, 4u }, // pa3
	{ 0x20336175U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // ua3
	{ 0x20336177U, GearSlot::Shield, NonGearKind::None, true, false, 12u, 2u, 2u }, // wa3
	{ 0x20336178U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xa3
	{ 0x2033626FU, GearSlot::Orb, NonGearKind::None, true, false, 8u, 1u, 2u }, // ob3
	{ 0x2033656EU, GearSlot::Shield, NonGearKind::None, true, false, 12u, 2u, 2u }, // ne3
	{ 0x20336963U, GearSlot::Helm, NonGearKind::None, true, false, 64u, 2u, 2u }, // ci3
	{ 0x20336B70U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // pk3
	{ 0x20336D61U, GearSlot::Spear, NonGearKind::None, true, true, 14u, 2u, 4u }, // am3
	{ 0x20336D63U, GearSlot::None, NonGearKind::Charm, false, false, 0u, 1u, 3u }, // cm3
	{ 0x20336D75U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // um3
	{ 0x20337068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hp3
	{ 0x2033706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mp3
	{ 0x20337264U, GearSlot::Helm, NonGearKind::None, true, false, 12u, 2u, 2u }, // dr3
	{ 0x20343073U, GearSlot::None, NonGearKind::None, true, false, 13u, 1u, 1u }, // s04
	{ 0x20343173U, GearSlot::None, NonGearKind::None, true, false, 31u, 1u, 1u }, // s14
	{ 0x20343273U, GearSlot::None, NonGearKind::None, true, false, 51u, 1u, 1u }, // s24
	{ 0x20343367U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // g34
	{ 0x2034336AU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // j34
	{ 0x20346162U, GearSlot::Helm, NonGearKind::None, true, false, 15u, 2u, 2u }, // ba4
	{ 0x20346170U, GearSlot::Shield, NonGearKind::None, true, false, 15u, 2u, 4u }, // pa4
	{ 0x20346175U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // ua4
	{ 0x20346177U, GearSlot::Shield, NonGearKind::None, true, false, 15u, 2u, 2u }, // wa4
	{ 0x20346178U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xa4
	{ 0x2034626FU, GearSlot::Orb, NonGearKind::None, true, false, 13u, 1u, 2u }, // ob4
	{ 0x2034656EU, GearSlot::Shield, NonGearKind::None, true, false, 15u, 2u, 2u }, // ne4
	{ 0x20346D61U, GearSlot::Spear, NonGearKind::None, true, true, 20u, 2u, 4u }, // am4
	{ 0x20346D63U, GearSlot::None, NonGearKind::Charm, false, false, 1u, 1u, 1u }, // cm4
	{ 0x20346D75U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // um4
	{ 0x20347068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hp4
	{ 0x2034706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mp4
	{ 0x20347264U, GearSlot::Helm, NonGearKind::None, true, false, 15u, 2u, 2u }, // dr4
	{ 0x20353073U, GearSlot::None, NonGearKind::None, true, false, 15u, 1u, 1u }, // s05
	{ 0x20353173U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // s15
	{ 0x20353273U, GearSlot::None, NonGearKind::None, true, false, 53u, 1u, 1u }, // s25
	{ 0x20356162U, GearSlot::Helm, NonGearKind::None, true, false, 18u, 2u, 2u }, // ba5
	{ 0x20356170U, GearSlot::Shield, NonGearKind::None, true, false, 18u, 2u, 2u }, // pa5
	{ 0x20356175U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // ua5
	{ 0x20356177U, GearSlot::Shield, NonGearKind::None, true, false, 18u, 2u, 2u }, // wa5
	{ 0x20356178U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xa5
	{ 0x2035626FU, GearSlot::Orb, NonGearKind::None, true, false, 18u, 1u, 3u }, // ob5
	{ 0x2035656EU, GearSlot::Shield, NonGearKind::None, true, false, 18u, 2u, 2u }, // ne5
	{ 0x20356D61U, GearSlot::Javelin, NonGearKind::None, true, false, 17u, 1u, 3u }, // am5
	{ 0x20356D63U, GearSlot::None, NonGearKind::Charm, false, false, 1u, 1u, 1u }, // cm5
	{ 0x20356D75U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // um5
	{ 0x20357068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hp5
	{ 0x2035706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mp5
	{ 0x20357264U, GearSlot::Helm, NonGearKind::None, true, false, 18u, 2u, 2u }, // dr5
	{ 0x20363073U, GearSlot::None, NonGearKind::None, true, false, 15u, 1u, 1u }, // s06
	{ 0x20363173U, GearSlot::None, NonGearKind::None, true, false, 35u, 1u, 1u }, // s16
	{ 0x20363273U, GearSlot::None, NonGearKind::None, true, false, 55u, 1u, 1u }, // s26
	{ 0x20366162U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // ba6
	{ 0x20366170U, GearSlot::Shield, NonGearKind::None, true, false, 26u, 2u, 2u }, // pa6
	{ 0x20366177U, GearSlot::Shield, NonGearKind::None, true, false, 24u, 2u, 2u }, // wa6
	{ 0x2036626FU, GearSlot::Orb, NonGearKind::None, true, false, 24u, 1u, 2u }, // ob6
	{ 0x2036656EU, GearSlot::Shield, NonGearKind::None, true, false, 24u, 2u, 2u }, // ne6
	{ 0x20366D61U, GearSlot::Bow, NonGearKind::None, true, true, 29u, 2u, 4u }, // am6
	{ 0x20366D75U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // um6
	{ 0x20367264U, GearSlot::Helm, NonGearKind::None, true, false, 26u, 2u, 2u }, // dr6
	{ 0x20373073U, GearSlot::None, NonGearKind::None, true, false, 17u, 1u, 1u }, // s07
	{ 0x20373173U, GearSlot::None, NonGearKind::None, true, false, 37u, 1u, 1u }, // s17
	{ 0x20373273U, GearSlot::None, NonGearKind::None, true, false, 57u, 1u, 1u }, // s27
	{ 0x20376162U, GearSlot::Helm, NonGearKind::None, true, false, 29u, 2u, 2u }, // ba7
	{ 0x20376170U, GearSlot::Shield, NonGearKind::None, true, false, 30u, 2u, 2u }, // pa7
	{ 0x20376177U, GearSlot::Shield, NonGearKind::None, true, false, 29u, 2u, 2u }, // wa7
	{ 0x20376237U, GearSlot::Sword, NonGearKind::None, true, true, 57u, 1u, 4u }, // 7b7
	{ 0x20376239U, GearSlot::Polearm, NonGearKind::None, true, true, 21u, 2u, 4u }, // 9b7
	{ 0x2037626FU, GearSlot::Orb, NonGearKind::None, true, false, 27u, 1u, 2u }, // ob7
	{ 0x2037656EU, GearSlot::Shield, NonGearKind::None, true, false, 29u, 2u, 2u }, // ne7
	{ 0x20376837U, GearSlot::Polearm, NonGearKind::None, true, true, 63u, 2u, 4u }, // 7h7
	{ 0x20376C36U, GearSlot::Bow, NonGearKind::None, true, true, 57u, 2u, 4u }, // 6l7
	{ 0x20376D37U, GearSlot::ClubMace, NonGearKind::None, true, true, 51u, 2u, 4u }, // 7m7
	{ 0x20376D61U, GearSlot::Bow, NonGearKind::None, true, true, 35u, 2u, 4u }, // am7
	{ 0x20376F37U, GearSlot::Polearm, NonGearKind::None, true, true, 45u, 2u, 4u }, // 7o7
	{ 0x20377037U, GearSlot::Spear, NonGearKind::None, true, true, 66u, 2u, 4u }, // 7p7
	{ 0x20377264U, GearSlot::Helm, NonGearKind::None, true, false, 30u, 2u, 2u }, // dr7
	{ 0x20377336U, GearSlot::Bow, NonGearKind::None, true, true, 54u, 2u, 3u }, // 6s7
	{ 0x20377337U, GearSlot::Javelin, NonGearKind::None, true, false, 53u, 1u, 3u }, // 7s7
	{ 0x20383073U, GearSlot::None, NonGearKind::None, true, false, 19u, 1u, 1u }, // s08
	{ 0x20383173U, GearSlot::None, NonGearKind::None, true, false, 39u, 1u, 1u }, // s18
	{ 0x20383273U, GearSlot::None, NonGearKind::None, true, false, 59u, 1u, 1u }, // s28
	{ 0x20386162U, GearSlot::Helm, NonGearKind::None, true, false, 29u, 2u, 2u }, // ba8
	{ 0x20386170U, GearSlot::Shield, NonGearKind::None, true, false, 34u, 2u, 4u }, // pa8
	{ 0x20386177U, GearSlot::Shield, NonGearKind::None, true, false, 33u, 2u, 2u }, // wa8
	{ 0x20386237U, GearSlot::Axe, NonGearKind::None, true, false, 60u, 2u, 3u }, // 7b8
	{ 0x20386239U, GearSlot::Axe, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9b8
	{ 0x2038626FU, GearSlot::Orb, NonGearKind::None, true, false, 30u, 1u, 2u }, // ob8
	{ 0x2038656EU, GearSlot::Shield, NonGearKind::None, true, false, 33u, 2u, 2u }, // ne8
	{ 0x20386C38U, GearSlot::Bow, NonGearKind::None, true, true, 25u, 2u, 4u }, // 8l8
	{ 0x20386D61U, GearSlot::Spear, NonGearKind::None, true, true, 32u, 2u, 4u }, // am8
	{ 0x20387264U, GearSlot::Helm, NonGearKind::None, true, false, 29u, 2u, 2u }, // dr8
	{ 0x20387337U, GearSlot::Polearm, NonGearKind::None, true, true, 53u, 2u, 4u }, // 7s8
	{ 0x20387338U, GearSlot::Bow, NonGearKind::None, true, true, 25u, 2u, 3u }, // 8s8
	{ 0x20387339U, GearSlot::Polearm, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9s8
	{ 0x20393073U, GearSlot::None, NonGearKind::None, true, false, 21u, 1u, 1u }, // s09
	{ 0x20393173U, GearSlot::None, NonGearKind::None, true, false, 41u, 1u, 1u }, // s19
	{ 0x20393273U, GearSlot::None, NonGearKind::None, true, false, 61u, 1u, 1u }, // s29
	{ 0x20396162U, GearSlot::Helm, NonGearKind::None, true, false, 32u, 2u, 2u }, // ba9
	{ 0x20396170U, GearSlot::Shield, NonGearKind::None, true, false, 38u, 2u, 4u }, // pa9
	{ 0x20396177U, GearSlot::Shield, NonGearKind::None, true, false, 36u, 2u, 2u }, // wa9
	{ 0x20396239U, GearSlot::Sword, NonGearKind::None, true, true, 25u, 1u, 4u }, // 9b9
	{ 0x2039626FU, GearSlot::Orb, NonGearKind::None, true, false, 34u, 1u, 2u }, // ob9
	{ 0x2039656EU, GearSlot::Shield, NonGearKind::None, true, false, 36u, 2u, 2u }, // ne9
	{ 0x20396839U, GearSlot::Polearm, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9h9
	{ 0x20396875U, GearSlot::Helm, NonGearKind::None, true, false, 63u, 2u, 2u }, // uh9
	{ 0x20396878U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xh9
	{ 0x20396D39U, GearSlot::ClubMace, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9m9
	{ 0x20396D61U, GearSlot::Spear, NonGearKind::None, true, true, 38u, 2u, 4u }, // am9
	{ 0x20397039U, GearSlot::Spear, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9p9
	{ 0x20397264U, GearSlot::Helm, NonGearKind::None, true, false, 32u, 2u, 2u }, // dr9
	{ 0x20397339U, GearSlot::Javelin, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9s9
	{ 0x20613237U, GearSlot::Axe, NonGearKind::None, true, false, 52u, 2u, 3u }, // 72a
	{ 0x20613239U, GearSlot::Axe, NonGearKind::None, true, false, 25u, 2u, 3u }, // 92a
	{ 0x20616162U, GearSlot::Helm, NonGearKind::None, true, false, 40u, 2u, 2u }, // baa
	{ 0x20616170U, GearSlot::Shield, NonGearKind::None, true, false, 41u, 2u, 2u }, // paa
	{ 0x20616177U, GearSlot::Shield, NonGearKind::None, true, false, 40u, 2u, 2u }, // waa
	{ 0x20616237U, GearSlot::Axe, NonGearKind::None, true, true, 48u, 2u, 3u }, // 7ba
	{ 0x20616239U, GearSlot::Axe, NonGearKind::None, true, true, 25u, 2u, 3u }, // 9ba
	{ 0x2061626FU, GearSlot::Orb, NonGearKind::None, true, false, 37u, 1u, 3u }, // oba
	{ 0x2061656CU, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // lea
	{ 0x2061656EU, GearSlot::Shield, NonGearKind::None, true, false, 40u, 2u, 2u }, // nea
	{ 0x20616575U, GearSlot::Torso, NonGearKind::None, true, false, 50u, 2u, 3u }, // uea
	{ 0x20616578U, GearSlot::Torso, NonGearKind::None, true, false, 24u, 2u, 3u }, // xea
	{ 0x20616737U, GearSlot::Axe, NonGearKind::None, true, true, 61u, 2u, 4u }, // 7ga
	{ 0x20616739U, GearSlot::Axe, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9ga
	{ 0x20616837U, GearSlot::Axe, NonGearKind::None, true, false, 40u, 1u, 3u }, // 7ha
	{ 0x20616839U, GearSlot::Axe, NonGearKind::None, true, false, 19u, 1u, 3u }, // 9ha
	{ 0x20616964U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // dia
	{ 0x20616A37U, GearSlot::Javelin, NonGearKind::None, true, false, 40u, 1u, 3u }, // 7ja
	{ 0x20616A39U, GearSlot::Javelin, NonGearKind::None, true, false, 18u, 1u, 3u }, // 9ja
	{ 0x20616B31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1ka
	{ 0x20616C37U, GearSlot::Axe, NonGearKind::None, true, true, 42u, 2u, 3u }, // 7la
	{ 0x20616C39U, GearSlot::Axe, NonGearKind::None, true, true, 22u, 2u, 3u }, // 9la
	{ 0x20616C66U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 2u, 3u }, // fla
	{ 0x20616C68U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // hla
	{ 0x20616C75U, GearSlot::Torso, NonGearKind::None, true, false, 51u, 2u, 3u }, // ula
	{ 0x20616C78U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xla
	{ 0x20616D37U, GearSlot::ClubMace, NonGearKind::None, true, false, 47u, 1u, 3u }, // 7ma
	{ 0x20616D39U, GearSlot::ClubMace, NonGearKind::None, true, false, 23u, 1u, 3u }, // 9ma
	{ 0x20616D61U, GearSlot::Javelin, NonGearKind::None, true, false, 26u, 1u, 3u }, // ama
	{ 0x20616D67U, GearSlot::ClubMace, NonGearKind::None, true, true, 0u, 2u, 3u }, // gma
	{ 0x20616F31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1oa
	{ 0x20616F6FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ooa
	{ 0x20616F74U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // toa
	{ 0x20617037U, GearSlot::Polearm, NonGearKind::None, true, true, 59u, 2u, 4u }, // 7pa
	{ 0x20617039U, GearSlot::Polearm, NonGearKind::None, true, true, 23u, 2u, 4u }, // 9pa
	{ 0x2061706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mpa
	{ 0x20617264U, GearSlot::Helm, NonGearKind::None, true, false, 41u, 2u, 2u }, // dra
	{ 0x20617331U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1sa
	{ 0x20617437U, GearSlot::Axe, NonGearKind::None, true, false, 42u, 1u, 2u }, // 7ta
	{ 0x20617439U, GearSlot::Axe, NonGearKind::None, true, false, 22u, 1u, 2u }, // 9ta
	{ 0x20617737U, GearSlot::Axe, NonGearKind::None, true, false, 64u, 2u, 3u }, // 7wa
	{ 0x20617739U, GearSlot::Axe, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9wa
	{ 0x20626162U, GearSlot::Helm, NonGearKind::None, true, false, 45u, 2u, 2u }, // bab
	{ 0x20626170U, GearSlot::Shield, NonGearKind::None, true, false, 47u, 2u, 2u }, // pab
	{ 0x20626177U, GearSlot::Shield, NonGearKind::None, true, false, 44u, 2u, 2u }, // wab
	{ 0x20626262U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // bbb
	{ 0x2062626CU, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 4u }, // lbb
	{ 0x2062626FU, GearSlot::Orb, NonGearKind::None, true, false, 44u, 1u, 2u }, // obb
	{ 0x20626273U, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 3u }, // sbb
	{ 0x20626336U, GearSlot::Bow, NonGearKind::None, true, true, 51u, 2u, 3u }, // 6cb
	{ 0x20626338U, GearSlot::Bow, NonGearKind::None, true, true, 25u, 2u, 3u }, // 8cb
	{ 0x2062656EU, GearSlot::Shield, NonGearKind::None, true, false, 44u, 2u, 2u }, // neb
	{ 0x20626637U, GearSlot::Sword, NonGearKind::None, true, true, 60u, 2u, 4u }, // 7fb
	{ 0x20626639U, GearSlot::Sword, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9fb
	{ 0x20626836U, GearSlot::Bow, NonGearKind::None, true, true, 45u, 2u, 3u }, // 6hb
	{ 0x20626838U, GearSlot::Bow, NonGearKind::None, true, true, 21u, 2u, 3u }, // 8hb
	{ 0x20626875U, GearSlot::Boots, NonGearKind::None, true, false, 65u, 2u, 2u }, // uhb
	{ 0x20626878U, GearSlot::Boots, NonGearKind::None, true, false, 25u, 2u, 2u }, // xhb
	{ 0x2062687AU, GearSlot::Belt, NonGearKind::None, true, false, 25u, 2u, 1u }, // zhb
	{ 0x20626C36U, GearSlot::Bow, NonGearKind::None, true, true, 47u, 2u, 4u }, // 6lb
	{ 0x20626C38U, GearSlot::Bow, NonGearKind::None, true, true, 23u, 2u, 4u }, // 8lb
	{ 0x20626C63U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 1u, 3u }, // clb
	{ 0x20626C66U, GearSlot::Sword, NonGearKind::None, true, true, 0u, 2u, 4u }, // flb
	{ 0x20626C75U, GearSlot::Boots, NonGearKind::None, true, false, 45u, 2u, 2u }, // ulb
	{ 0x20626C78U, GearSlot::Boots, NonGearKind::None, true, false, 24u, 2u, 2u }, // xlb
	{ 0x20626C7AU, GearSlot::Belt, NonGearKind::None, true, false, 24u, 2u, 1u }, // zlb
	{ 0x20626D61U, GearSlot::Bow, NonGearKind::None, true, true, 39u, 2u, 4u }, // amb
	{ 0x20626D75U, GearSlot::Boots, NonGearKind::None, true, false, 54u, 2u, 2u }, // umb
	{ 0x20626D78U, GearSlot::Boots, NonGearKind::None, true, false, 25u, 2u, 2u }, // xmb
	{ 0x20626D7AU, GearSlot::Belt, NonGearKind::None, true, false, 25u, 2u, 1u }, // zmb
	{ 0x20627231U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1rb
	{ 0x20627264U, GearSlot::Helm, NonGearKind::None, true, false, 46u, 2u, 2u }, // drb
	{ 0x20627268U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hrb
	{ 0x20627277U, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // wrb
	{ 0x20627336U, GearSlot::Bow, NonGearKind::None, true, true, 41u, 2u, 3u }, // 6sb
	{ 0x20627337U, GearSlot::Sword, NonGearKind::None, true, false, 47u, 1u, 3u }, // 7sb
	{ 0x20627338U, GearSlot::Bow, NonGearKind::None, true, true, 18u, 2u, 3u }, // 8sb
	{ 0x20627339U, GearSlot::Sword, NonGearKind::None, true, false, 23u, 1u, 3u }, // 9sb
	{ 0x20627475U, GearSlot::Boots, NonGearKind::None, true, false, 60u, 2u, 2u }, // utb
	{ 0x20627478U, GearSlot::Boots, NonGearKind::None, true, false, 25u, 2u, 2u }, // xtb
	{ 0x2062747AU, GearSlot::Belt, NonGearKind::None, true, false, 25u, 2u, 1u }, // ztb
	{ 0x20627675U, GearSlot::Boots, NonGearKind::None, true, false, 49u, 2u, 2u }, // uvb
	{ 0x20627678U, GearSlot::Boots, NonGearKind::None, true, false, 25u, 2u, 2u }, // xvb
	{ 0x2062767AU, GearSlot::Belt, NonGearKind::None, true, false, 25u, 2u, 1u }, // zvb
	{ 0x20627737U, GearSlot::Claw, NonGearKind::None, true, false, 46u, 1u, 3u }, // 7wb
	{ 0x20627739U, GearSlot::Claw, NonGearKind::None, true, false, 24u, 1u, 3u }, // 9wb
	{ 0x2062776CU, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 4u }, // lwb
	{ 0x20627773U, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 3u }, // swb
	{ 0x20627868U, GearSlot::Crossbow, NonGearKind::None, true, true, 0u, 2u, 4u }, // hxb
	{ 0x2062786CU, GearSlot::Crossbow, NonGearKind::None, true, true, 0u, 2u, 3u }, // lxb
	{ 0x2062786DU, GearSlot::Crossbow, NonGearKind::None, true, true, 0u, 2u, 3u }, // mxb
	{ 0x20627872U, GearSlot::Crossbow, NonGearKind::None, true, true, 0u, 2u, 3u }, // rxb
	{ 0x20636162U, GearSlot::Helm, NonGearKind::None, true, false, 49u, 2u, 2u }, // bac
	{ 0x2063616DU, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 1u, 3u }, // mac
	{ 0x20636170U, GearSlot::Shield, NonGearKind::None, true, false, 52u, 2u, 2u }, // pac
	{ 0x20636177U, GearSlot::Shield, NonGearKind::None, true, false, 49u, 2u, 2u }, // wac
	{ 0x2063626FU, GearSlot::Orb, NonGearKind::None, true, false, 50u, 1u, 2u }, // obc
	{ 0x20636637U, GearSlot::Sword, NonGearKind::None, true, false, 51u, 1u, 3u }, // 7fc
	{ 0x20636639U, GearSlot::Sword, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9fc
	{ 0x20636731U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1gc
	{ 0x20636875U, GearSlot::Belt, NonGearKind::None, true, false, 67u, 2u, 1u }, // uhc
	{ 0x20636C62U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // blc
	{ 0x20636C66U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 1u, 3u }, // flc
	{ 0x20636C75U, GearSlot::Belt, NonGearKind::None, true, false, 46u, 2u, 1u }, // ulc
	{ 0x20636D61U, GearSlot::Bow, NonGearKind::None, true, true, 58u, 2u, 4u }, // amc
	{ 0x20636D75U, GearSlot::Belt, NonGearKind::None, true, false, 56u, 2u, 1u }, // umc
	{ 0x20636F31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1oc
	{ 0x20636F6FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ooc
	{ 0x20637073U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 1u, 3u }, // spc
	{ 0x20637264U, GearSlot::Helm, NonGearKind::None, true, false, 51u, 2u, 2u }, // drc
	{ 0x20637330U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 0sc
	{ 0x20637337U, GearSlot::Scepter, NonGearKind::None, true, false, 46u, 1u, 3u }, // 7sc
	{ 0x20637339U, GearSlot::Scepter, NonGearKind::None, true, false, 19u, 1u, 3u }, // 9sc
	{ 0x20637367U, GearSlot::Scepter, NonGearKind::None, true, false, 0u, 1u, 3u }, // gsc
	{ 0x20637369U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // isc
	{ 0x20637374U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // tsc
	{ 0x20637377U, GearSlot::Polearm, NonGearKind::None, true, true, 0u, 2u, 4u }, // wsc
	{ 0x20637475U, GearSlot::Belt, NonGearKind::None, true, false, 62u, 2u, 1u }, // utc
	{ 0x20637562U, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 2u }, // buc
	{ 0x20637575U, GearSlot::Shield, NonGearKind::None, true, false, 43u, 2u, 2u }, // uuc
	{ 0x20637578U, GearSlot::Shield, NonGearKind::None, true, false, 22u, 2u, 2u }, // xuc
	{ 0x20637675U, GearSlot::Belt, NonGearKind::None, true, false, 51u, 2u, 1u }, // uvc
	{ 0x20637737U, GearSlot::Polearm, NonGearKind::None, true, true, 66u, 2u, 4u }, // 7wc
	{ 0x20637739U, GearSlot::Polearm, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9wc
	{ 0x20646162U, GearSlot::Helm, NonGearKind::None, true, false, 54u, 2u, 2u }, // bad
	{ 0x20646170U, GearSlot::Shield, NonGearKind::None, true, false, 55u, 2u, 4u }, // pad
	{ 0x20646177U, GearSlot::Shield, NonGearKind::None, true, false, 50u, 2u, 2u }, // wad
	{ 0x20646261U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // abd
	{ 0x2064626FU, GearSlot::Orb, NonGearKind::None, true, false, 56u, 1u, 2u }, // obd
	{ 0x20646462U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // bdd
	{ 0x20646566U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // fed
	{ 0x2064656EU, GearSlot::Shield, NonGearKind::None, true, false, 49u, 2u, 2u }, // ned
	{ 0x20646737U, GearSlot::Sword, NonGearKind::None, true, true, 63u, 2u, 4u }, // 7gd
	{ 0x20646739U, GearSlot::Sword, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9gd
	{ 0x2064676EU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ngd
	{ 0x20646B62U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // bkd
	{ 0x20646C62U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 3u }, // bld
	{ 0x20646C66U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // fld
	{ 0x20646C67U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // gld
	{ 0x20646C75U, GearSlot::Torso, NonGearKind::None, true, false, 61u, 2u, 3u }, // uld
	{ 0x20646C78U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xld
	{ 0x20646D61U, GearSlot::Spear, NonGearKind::None, true, true, 45u, 2u, 4u }, // amd
	{ 0x20646E77U, GearSlot::Wand, NonGearKind::None, true, false, 0u, 1u, 2u }, // wnd
	{ 0x20646F68U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hod
	{ 0x20647264U, GearSlot::Helm, NonGearKind::None, true, false, 57u, 2u, 2u }, // drd
	{ 0x20647269U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ird
	{ 0x20647362U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 2u, 3u }, // bsd
	{ 0x20647367U, GearSlot::Sword, NonGearKind::None, true, true, 0u, 2u, 4u }, // gsd
	{ 0x2064736CU, GearSlot::Sword, NonGearKind::None, true, false, 0u, 2u, 3u }, // lsd
	{ 0x20647373U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 1u, 3u }, // ssd
	{ 0x20647377U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 1u, 3u }, // wsd
	{ 0x20647473U, GearSlot::None, NonGearKind::None, true, false, 90u, 1u, 1u }, // std
	{ 0x20647474U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ttd
	{ 0x20647737U, GearSlot::Sword, NonGearKind::None, true, false, 66u, 1u, 3u }, // 7wd
	{ 0x20647739U, GearSlot::Sword, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9wd
	{ 0x20647764U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // dwd
	{ 0x20647963U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // cyd
	{ 0x20647972U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ryd
	{ 0x20656162U, GearSlot::Helm, NonGearKind::None, true, false, 60u, 2u, 2u }, // bae
	{ 0x20656170U, GearSlot::Shield, NonGearKind::None, true, false, 61u, 2u, 4u }, // pae
	{ 0x20656177U, GearSlot::Shield, NonGearKind::None, true, false, 60u, 2u, 2u }, // wae
	{ 0x2065626FU, GearSlot::Orb, NonGearKind::None, true, false, 63u, 1u, 2u }, // obe
	{ 0x20656369U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ice
	{ 0x2065656EU, GearSlot::Shield, NonGearKind::None, true, false, 60u, 2u, 2u }, // nee
	{ 0x20656D61U, GearSlot::Spear, NonGearKind::None, true, true, 60u, 2u, 4u }, // ame
	{ 0x20656F31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1oe
	{ 0x20656F6FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ooe
	{ 0x20657073U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // spe
	{ 0x20657264U, GearSlot::Helm, NonGearKind::None, true, false, 62u, 2u, 2u }, // dre
	{ 0x20657861U, GearSlot::Axe, NonGearKind::None, true, false, 0u, 2u, 3u }, // axe
	{ 0x20666162U, GearSlot::Helm, NonGearKind::None, true, false, 65u, 2u, 2u }, // baf
	{ 0x20666170U, GearSlot::Shield, NonGearKind::None, true, false, 66u, 2u, 2u }, // paf
	{ 0x20666177U, GearSlot::Shield, NonGearKind::None, true, false, 65u, 2u, 2u }, // waf
	{ 0x2066626FU, GearSlot::Orb, NonGearKind::None, true, false, 66u, 1u, 3u }, // obf
	{ 0x2066656EU, GearSlot::Shield, NonGearKind::None, true, false, 65u, 2u, 2u }, // nef
	{ 0x20666B62U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 2u }, // bkf
	{ 0x20666B74U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 2u }, // tkf
	{ 0x20666D61U, GearSlot::Javelin, NonGearKind::None, true, false, 48u, 1u, 3u }, // amf
	{ 0x20667068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hpf
	{ 0x2066706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mpf
	{ 0x20667264U, GearSlot::Helm, NonGearKind::None, true, false, 66u, 2u, 2u }, // drf
	{ 0x2066736DU, GearSlot::Staff, NonGearKind::None, true, true, 0u, 1u, 3u }, // msf
	{ 0x20667837U, GearSlot::Claw, NonGearKind::None, true, false, 51u, 1u, 3u }, // 7xf
	{ 0x20667839U, GearSlot::Claw, NonGearKind::None, true, false, 27u, 1u, 3u }, // 9xf
	{ 0x20667861U, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // axf
	{ 0x20676162U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // bag
	{ 0x20676437U, GearSlot::Knife, NonGearKind::None, true, false, 43u, 1u, 2u }, // 7dg
	{ 0x20676439U, GearSlot::Knife, NonGearKind::None, true, false, 19u, 1u, 2u }, // 9dg
	{ 0x2067656CU, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 1u, 3u }, // leg
	{ 0x2067656EU, GearSlot::Shield, NonGearKind::None, true, false, 50u, 2u, 2u }, // neg
	{ 0x20676875U, GearSlot::Gloves, NonGearKind::None, true, false, 64u, 2u, 2u }, // uhg
	{ 0x20676878U, GearSlot::Gloves, NonGearKind::None, true, false, 25u, 2u, 2u }, // xhg
	{ 0x20676B64U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // dkg
	{ 0x20676B68U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hkg
	{ 0x20676B74U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // tkg
	{ 0x20676C66U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // flg
	{ 0x20676C75U, GearSlot::Gloves, NonGearKind::None, true, false, 42u, 2u, 2u }, // ulg
	{ 0x20676C78U, GearSlot::Gloves, NonGearKind::None, true, false, 21u, 2u, 2u }, // xlg
	{ 0x20676D75U, GearSlot::Gloves, NonGearKind::None, true, false, 51u, 2u, 2u }, // umg
	{ 0x20676D78U, GearSlot::Gloves, NonGearKind::None, true, false, 25u, 2u, 2u }, // xmg
	{ 0x20676E66U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // fng
	{ 0x20676E72U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // rng
	{ 0x20676E75U, GearSlot::Torso, NonGearKind::None, true, false, 54u, 2u, 3u }, // ung
	{ 0x20676E78U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xng
	{ 0x2067726CU, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 3u }, // lrg
	{ 0x20677275U, GearSlot::Shield, NonGearKind::None, true, false, 54u, 2u, 3u }, // urg
	{ 0x20677278U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 3u }, // xrg
	{ 0x20677475U, GearSlot::Gloves, NonGearKind::None, true, false, 57u, 2u, 2u }, // utg
	{ 0x20677478U, GearSlot::Gloves, NonGearKind::None, true, false, 25u, 2u, 2u }, // xtg
	{ 0x20677675U, GearSlot::Gloves, NonGearKind::None, true, false, 47u, 2u, 2u }, // uvg
	{ 0x20677678U, GearSlot::Gloves, NonGearKind::None, true, false, 25u, 2u, 2u }, // xvg
	{ 0x20683237U, GearSlot::Sword, NonGearKind::None, true, true, 44u, 1u, 4u }, // 72h
	{ 0x20683239U, GearSlot::Sword, NonGearKind::None, true, true, 25u, 1u, 4u }, // 92h
	{ 0x2068636BU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // kch
	{ 0x20686374U, GearSlot::None, NonGearKind::Charm, false, false, 0u, 1u, 2u }, // tch
	{ 0x20686563U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ceh
	{ 0x20686668U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 2u, 3u }, // hfh
	{ 0x20687362U, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 3u }, // bsh
	{ 0x20687375U, GearSlot::Shield, NonGearKind::None, true, false, 57u, 2u, 3u }, // ush
	{ 0x20687378U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 3u }, // xsh
	{ 0x20687467U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // gth
	{ 0x20687475U, GearSlot::Torso, NonGearKind::None, true, false, 62u, 2u, 3u }, // uth
	{ 0x20687478U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xth
	{ 0x20687737U, GearSlot::ClubMace, NonGearKind::None, true, false, 61u, 2u, 3u }, // 7wh
	{ 0x20687739U, GearSlot::ClubMace, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9wh
	{ 0x20696431U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1di
	{ 0x20696437U, GearSlot::Knife, NonGearKind::None, true, false, 52u, 1u, 2u }, // 7di
	{ 0x20696439U, GearSlot::Knife, NonGearKind::None, true, false, 24u, 1u, 2u }, // 9di
	{ 0x20696737U, GearSlot::Axe, NonGearKind::None, true, true, 66u, 2u, 3u }, // 7gi
	{ 0x20696739U, GearSlot::Axe, NonGearKind::None, true, true, 25u, 2u, 3u }, // 9gi
	{ 0x20696F31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1oi
	{ 0x20696F6FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ooi
	{ 0x20697037U, GearSlot::Javelin, NonGearKind::None, true, false, 46u, 1u, 3u }, // 7pi
	{ 0x20697039U, GearSlot::Javelin, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9pi
	{ 0x2069706DU, GearSlot::Axe, NonGearKind::None, true, false, 0u, 2u, 3u }, // mpi
	{ 0x2069726BU, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 3u }, // kri
	{ 0x20697274U, GearSlot::Spear, NonGearKind::None, true, true, 0u, 2u, 4u }, // tri
	{ 0x20697571U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // qui
	{ 0x20697575U, GearSlot::Torso, NonGearKind::None, true, false, 49u, 2u, 3u }, // uui
	{ 0x20697578U, GearSlot::Torso, NonGearKind::None, true, false, 22u, 2u, 3u }, // xui
	{ 0x206B6237U, GearSlot::Knife, NonGearKind::None, true, false, 57u, 1u, 2u }, // 7bk
	{ 0x206B6239U, GearSlot::Knife, NonGearKind::None, true, false, 25u, 1u, 2u }, // 9bk
	{ 0x206B6269U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // ibk
	{ 0x206B6274U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // tbk
	{ 0x206B6970U, GearSlot::Spear, NonGearKind::None, true, true, 0u, 2u, 4u }, // pik
	{ 0x206B7073U, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 3u }, // spk
	{ 0x206B7075U, GearSlot::Shield, NonGearKind::None, true, false, 51u, 2u, 3u }, // upk
	{ 0x206B7078U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 3u }, // xpk
	{ 0x206B7331U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1sk
	{ 0x206B736DU, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // msk
	{ 0x206B7375U, GearSlot::Helm, NonGearKind::None, true, false, 55u, 2u, 2u }, // usk
	{ 0x206B7378U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xsk
	{ 0x206B7437U, GearSlot::Knife, NonGearKind::None, true, false, 48u, 1u, 2u }, // 7tk
	{ 0x206B7439U, GearSlot::Knife, NonGearKind::None, true, false, 19u, 1u, 2u }, // 9tk
	{ 0x206C6162U, GearSlot::Axe, NonGearKind::None, true, false, 0u, 2u, 3u }, // bal
	{ 0x206C6168U, GearSlot::Polearm, NonGearKind::None, true, true, 0u, 2u, 4u }, // hal
	{ 0x206C6174U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // tal
	{ 0x206C6237U, GearSlot::Knife, NonGearKind::None, true, false, 66u, 1u, 3u }, // 7bl
	{ 0x206C6239U, GearSlot::Knife, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9bl
	{ 0x206C6268U, GearSlot::Belt, NonGearKind::None, true, false, 0u, 2u, 1u }, // hbl
	{ 0x206C626CU, GearSlot::Belt, NonGearKind::None, true, false, 0u, 2u, 1u }, // lbl
	{ 0x206C626DU, GearSlot::Belt, NonGearKind::None, true, false, 0u, 2u, 1u }, // mbl
	{ 0x206C6274U, GearSlot::Belt, NonGearKind::None, true, false, 0u, 2u, 1u }, // tbl
	{ 0x206C6276U, GearSlot::Belt, NonGearKind::None, true, false, 0u, 2u, 1u }, // vbl
	{ 0x206C6337U, GearSlot::ClubMace, NonGearKind::None, true, false, 39u, 1u, 3u }, // 7cl
	{ 0x206C6339U, GearSlot::ClubMace, NonGearKind::None, true, false, 18u, 1u, 3u }, // 9cl
	{ 0x206C6373U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // scl
	{ 0x206C6375U, GearSlot::Torso, NonGearKind::None, true, false, 55u, 2u, 3u }, // ucl
	{ 0x206C6378U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xcl
	{ 0x206C6637U, GearSlot::ClubMace, NonGearKind::None, true, false, 57u, 2u, 3u }, // 7fl
	{ 0x206C6639U, GearSlot::ClubMace, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9fl
	{ 0x206C6737U, GearSlot::Javelin, NonGearKind::None, true, false, 59u, 1u, 4u }, // 7gl
	{ 0x206C6739U, GearSlot::Javelin, NonGearKind::None, true, false, 25u, 1u, 4u }, // 9gl
	{ 0x206C6768U, GearSlot::Gloves, NonGearKind::None, true, false, 0u, 2u, 2u }, // hgl
	{ 0x206C676CU, GearSlot::Gloves, NonGearKind::None, true, false, 0u, 2u, 2u }, // lgl
	{ 0x206C676DU, GearSlot::Gloves, NonGearKind::None, true, false, 0u, 2u, 2u }, // mgl
	{ 0x206C6774U, GearSlot::Gloves, NonGearKind::None, true, false, 0u, 2u, 2u }, // tgl
	{ 0x206C6776U, GearSlot::Gloves, NonGearKind::None, true, false, 0u, 2u, 2u }, // vgl
	{ 0x206C6866U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // fhl
	{ 0x206C6875U, GearSlot::Helm, NonGearKind::None, true, false, 40u, 2u, 2u }, // uhl
	{ 0x206C6878U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xhl
	{ 0x206C6970U, GearSlot::Javelin, NonGearKind::None, true, false, 0u, 1u, 3u }, // pil
	{ 0x206C6C71U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // qll
	{ 0x206C6D73U, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 2u }, // sml
	{ 0x206C6D75U, GearSlot::Shield, NonGearKind::None, true, false, 45u, 2u, 2u }, // uml
	{ 0x206C6D78U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 2u }, // xml
	{ 0x206C6F73U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // sol
	{ 0x206C7062U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // bpl
	{ 0x206C7067U, GearSlot::OtherWeapon, NonGearKind::None, true, false, 6u, 1u, 1u }, // gpl
	{ 0x206C706FU, GearSlot::OtherWeapon, NonGearKind::None, true, false, 0u, 1u, 1u }, // opl
	{ 0x206C7072U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // rpl
	{ 0x206C7073U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // spl
	{ 0x206C7075U, GearSlot::Torso, NonGearKind::None, true, false, 57u, 2u, 3u }, // upl
	{ 0x206C7078U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xpl
	{ 0x206C7462U, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // btl
	{ 0x206C7566U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // ful
	{ 0x206C7575U, GearSlot::Torso, NonGearKind::None, true, false, 64u, 2u, 3u }, // uul
	{ 0x206C7578U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xul
	{ 0x206C7672U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // rvl
	{ 0x206D6131U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1am
	{ 0x206D6337U, GearSlot::Sword, NonGearKind::None, true, true, 49u, 1u, 4u }, // 7cm
	{ 0x206D6339U, GearSlot::Sword, NonGearKind::None, true, true, 25u, 1u, 4u }, // 9cm
	{ 0x206D6373U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 1u, 3u }, // scm
	{ 0x206D6468U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 1u, 2u }, // hdm
	{ 0x206D6531U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1em
	{ 0x206D6737U, GearSlot::ClubMace, NonGearKind::None, true, true, 65u, 2u, 3u }, // 7gm
	{ 0x206D6739U, GearSlot::ClubMace, NonGearKind::None, true, true, 25u, 2u, 3u }, // 9gm
	{ 0x206D6862U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // bhm
	{ 0x206D6867U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // ghm
	{ 0x206D6875U, GearSlot::Helm, NonGearKind::None, true, false, 59u, 2u, 2u }, // uhm
	{ 0x206D6877U, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 2u, 3u }, // whm
	{ 0x206D6878U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xhm
	{ 0x206D6C63U, GearSlot::Sword, NonGearKind::None, true, true, 0u, 1u, 4u }, // clm
	{ 0x206D6C68U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // hlm
	{ 0x206D6C75U, GearSlot::Helm, NonGearKind::None, true, false, 51u, 2u, 2u }, // ulm
	{ 0x206D6C78U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xlm
	{ 0x206D7067U, GearSlot::OtherWeapon, NonGearKind::None, true, false, 16u, 1u, 1u }, // gpm
	{ 0x206D706FU, GearSlot::OtherWeapon, NonGearKind::None, true, false, 12u, 1u, 1u }, // opm
	{ 0x206D7264U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // drm
	{ 0x206D7337U, GearSlot::Sword, NonGearKind::None, true, false, 45u, 1u, 3u }, // 7sm
	{ 0x206D7339U, GearSlot::Sword, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9sm
	{ 0x206E6863U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // chn
	{ 0x206E6864U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // dhn
	{ 0x206E6875U, GearSlot::Torso, NonGearKind::None, true, false, 47u, 2u, 3u }, // uhn
	{ 0x206E6878U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xhn
	{ 0x206E6972U, GearSlot::None, NonGearKind::Ring, false, false, 0u, 1u, 1u }, // rin
	{ 0x206E7262U, GearSlot::Spear, NonGearKind::None, true, true, 0u, 2u, 4u }, // brn
	{ 0x206E7263U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // crn
	{ 0x206E7268U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hrn
	{ 0x206E7275U, GearSlot::Helm, NonGearKind::None, true, false, 66u, 2u, 2u }, // urn
	{ 0x206E7278U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xrn
	{ 0x206E7737U, GearSlot::Wand, NonGearKind::None, true, false, 41u, 1u, 2u }, // 7wn
	{ 0x206E7739U, GearSlot::Wand, NonGearKind::None, true, false, 19u, 1u, 2u }, // 9wn
	{ 0x206E7762U, GearSlot::Wand, NonGearKind::None, true, false, 0u, 1u, 2u }, // bwn
	{ 0x206E7767U, GearSlot::Wand, NonGearKind::None, true, false, 0u, 1u, 2u }, // gwn
	{ 0x206E7779U, GearSlot::Wand, NonGearKind::None, true, false, 0u, 1u, 2u }, // ywn
	{ 0x206F7068U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hpo
	{ 0x206F706DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mpo
	{ 0x206F7331U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1so
	{ 0x206F7637U, GearSlot::Polearm, NonGearKind::None, true, true, 48u, 2u, 4u }, // 7vo
	{ 0x206F7639U, GearSlot::Polearm, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9vo
	{ 0x20706163U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // cap
	{ 0x20706175U, GearSlot::Helm, NonGearKind::None, true, false, 43u, 2u, 2u }, // uap
	{ 0x20706178U, GearSlot::Helm, NonGearKind::None, true, false, 22u, 2u, 2u }, // xap
	{ 0x20706373U, GearSlot::Scepter, NonGearKind::None, true, false, 0u, 1u, 3u }, // scp
	{ 0x2070666DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mfp
	{ 0x20706870U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // php
	{ 0x20706976U, GearSlot::None, NonGearKind::Amulet, false, false, 0u, 1u, 1u }, // vip
	{ 0x20706B73U, GearSlot::Helm, NonGearKind::None, true, false, 0u, 2u, 2u }, // skp
	{ 0x20706B75U, GearSlot::Helm, NonGearKind::None, true, false, 47u, 2u, 2u }, // ukp
	{ 0x20706B78U, GearSlot::Helm, NonGearKind::None, true, false, 25u, 2u, 2u }, // xkp
	{ 0x20706D37U, GearSlot::Axe, NonGearKind::None, true, false, 59u, 2u, 3u }, // 7mp
	{ 0x20706D39U, GearSlot::Axe, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9mp
	{ 0x20707078U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xpp
	{ 0x20707337U, GearSlot::ClubMace, NonGearKind::None, true, false, 42u, 1u, 3u }, // 7sp
	{ 0x20707339U, GearSlot::ClubMace, NonGearKind::None, true, false, 20u, 1u, 3u }, // 9sp
	{ 0x20707363U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // csp
	{ 0x20707366U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // fsp
	{ 0x2070736CU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // lsp
	{ 0x2070736DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // msp
	{ 0x20707370U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // psp
	{ 0x20707373U, GearSlot::Javelin, NonGearKind::None, true, false, 0u, 1u, 3u }, // ssp
	{ 0x20707374U, GearSlot::Javelin, NonGearKind::None, true, false, 0u, 1u, 4u }, // tsp
	{ 0x20707377U, GearSlot::Scepter, NonGearKind::None, true, false, 0u, 2u, 3u }, // wsp
	{ 0x20707431U, GearSlot::None, NonGearKind::None, true, false, 18u, 1u, 1u }, // 1tp
	{ 0x2070746CU, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // ltp
	{ 0x20707475U, GearSlot::Torso, NonGearKind::None, true, false, 63u, 2u, 3u }, // utp
	{ 0x20707478U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xtp
	{ 0x20707572U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // rup
	{ 0x2070776AU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // jwp
	{ 0x20726137U, GearSlot::Claw, NonGearKind::None, true, false, 44u, 1u, 3u }, // 7ar
	{ 0x20726139U, GearSlot::Claw, NonGearKind::None, true, false, 21u, 1u, 3u }, // 9ar
	{ 0x20726161U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // aar
	{ 0x20726162U, GearSlot::Polearm, NonGearKind::None, true, true, 0u, 2u, 4u }, // bar
	{ 0x20726165U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ear
	{ 0x20726175U, GearSlot::Torso, NonGearKind::None, true, false, 66u, 2u, 3u }, // uar
	{ 0x20726178U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xar
	{ 0x20726237U, GearSlot::Spear, NonGearKind::None, true, true, 55u, 2u, 4u }, // 7br
	{ 0x20726239U, GearSlot::Spear, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9br
	{ 0x2072626DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mbr
	{ 0x20726271U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // qbr
	{ 0x20726273U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 1u, 3u }, // sbr
	{ 0x20726337U, GearSlot::Sword, NonGearKind::None, true, false, 54u, 2u, 3u }, // 7cr
	{ 0x20726339U, GearSlot::Sword, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9cr
	{ 0x20726761U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // agr
	{ 0x20726764U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 2u }, // dgr
	{ 0x20726765U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // egr
	{ 0x2072676BU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // kgr
	{ 0x2072676DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mgr
	{ 0x2072676FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // ogr
	{ 0x20726772U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // rgr
	{ 0x20726773U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // sgr
	{ 0x20726774U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // tgr
	{ 0x20726871U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // qhr
	{ 0x20726964U, GearSlot::Knife, NonGearKind::None, true, false, 0u, 1u, 2u }, // dir
	{ 0x20726B37U, GearSlot::Knife, NonGearKind::None, true, false, 62u, 1u, 3u }, // 7kr
	{ 0x20726B39U, GearSlot::Knife, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9kr
	{ 0x20726B73U, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // skr
	{ 0x20726F31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1or
	{ 0x20726F6FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // oor
	{ 0x20727073U, GearSlot::Spear, NonGearKind::None, true, true, 0u, 2u, 4u }, // spr
	{ 0x20727137U, GearSlot::Claw, NonGearKind::None, true, false, 64u, 1u, 3u }, // 7qr
	{ 0x20727139U, GearSlot::Claw, NonGearKind::None, true, false, 40u, 1u, 3u }, // 9qr
	{ 0x20727337U, GearSlot::Spear, NonGearKind::None, true, true, 43u, 2u, 4u }, // 7sr
	{ 0x20727339U, GearSlot::Spear, NonGearKind::None, true, true, 21u, 2u, 4u }, // 9sr
	{ 0x20727437U, GearSlot::Spear, NonGearKind::None, true, true, 49u, 2u, 4u }, // 7tr
	{ 0x20727439U, GearSlot::Spear, NonGearKind::None, true, true, 24u, 2u, 4u }, // 9tr
	{ 0x2072746BU, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // ktr
	{ 0x20736236U, GearSlot::Staff, NonGearKind::None, true, true, 62u, 1u, 4u }, // 6bs
	{ 0x20736237U, GearSlot::Sword, NonGearKind::None, true, false, 58u, 2u, 3u }, // 7bs
	{ 0x20736238U, GearSlot::Staff, NonGearKind::None, true, true, 25u, 1u, 4u }, // 8bs
	{ 0x20736239U, GearSlot::Sword, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9bs
	{ 0x20736336U, GearSlot::Staff, NonGearKind::None, true, true, 55u, 1u, 4u }, // 6cs
	{ 0x20736337U, GearSlot::Claw, NonGearKind::None, true, false, 54u, 1u, 3u }, // 7cs
	{ 0x20736338U, GearSlot::Staff, NonGearKind::None, true, true, 25u, 1u, 4u }, // 8cs
	{ 0x20736339U, GearSlot::Claw, NonGearKind::None, true, false, 30u, 1u, 3u }, // 9cs
	{ 0x20736373U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // scs
	{ 0x20736563U, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // ces
	{ 0x20736574U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // tes
	{ 0x20736737U, GearSlot::Sword, NonGearKind::None, true, true, 53u, 1u, 4u }, // 7gs
	{ 0x20736739U, GearSlot::Sword, NonGearKind::None, true, true, 25u, 1u, 4u }, // 9gs
	{ 0x20736832U, GearSlot::Sword, NonGearKind::None, true, true, 0u, 1u, 4u }, // 2hs
	{ 0x20736967U, GearSlot::Sword, NonGearKind::None, true, true, 0u, 1u, 4u }, // gis
	{ 0x20736B62U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // bks
	{ 0x20736C36U, GearSlot::Staff, NonGearKind::None, true, true, 49u, 1u, 4u }, // 6ls
	{ 0x20736C37U, GearSlot::Sword, NonGearKind::None, true, false, 61u, 2u, 3u }, // 7ls
	{ 0x20736C38U, GearSlot::Staff, NonGearKind::None, true, true, 23u, 1u, 4u }, // 8ls
	{ 0x20736C39U, GearSlot::Sword, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9ls
	{ 0x20736D77U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // wms
	{ 0x20736F31U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // 1os
	{ 0x20736F6FU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // oos
	{ 0x20737062U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // bps
	{ 0x20737067U, GearSlot::OtherWeapon, NonGearKind::None, true, false, 24u, 1u, 1u }, // gps
	{ 0x2073706FU, GearSlot::OtherWeapon, NonGearKind::None, true, false, 20u, 1u, 1u }, // ops
	{ 0x20737072U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // rps
	{ 0x20737076U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // vps
	{ 0x20737079U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // yps
	{ 0x20737137U, GearSlot::Scepter, NonGearKind::None, true, false, 57u, 1u, 3u }, // 7qs
	{ 0x20737139U, GearSlot::Scepter, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9qs
	{ 0x20737262U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // brs
	{ 0x20737263U, GearSlot::Sword, NonGearKind::None, true, false, 0u, 2u, 3u }, // crs
	{ 0x20737275U, GearSlot::Torso, NonGearKind::None, true, false, 56u, 2u, 3u }, // urs
	{ 0x20737278U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xrs
	{ 0x20737336U, GearSlot::Staff, NonGearKind::None, true, true, 43u, 1u, 3u }, // 6ss
	{ 0x20737337U, GearSlot::Sword, NonGearKind::None, true, false, 42u, 1u, 3u }, // 7ss
	{ 0x20737338U, GearSlot::Staff, NonGearKind::None, true, true, 18u, 1u, 3u }, // 8ss
	{ 0x20737339U, GearSlot::Sword, NonGearKind::None, true, false, 18u, 1u, 3u }, // 9ss
	{ 0x20737361U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // ass
	{ 0x2073736DU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // mss
	{ 0x20737437U, GearSlot::Javelin, NonGearKind::None, true, false, 65u, 1u, 4u }, // 7ts
	{ 0x20737439U, GearSlot::Javelin, NonGearKind::None, true, false, 25u, 1u, 4u }, // 9ts
	{ 0x20737467U, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 4u }, // gts
	{ 0x20737475U, GearSlot::Shield, NonGearKind::None, true, false, 63u, 2u, 4u }, // uts
	{ 0x20737478U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 4u }, // xts
	{ 0x20737672U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // rvs
	{ 0x20737736U, GearSlot::Staff, NonGearKind::None, true, true, 66u, 2u, 4u }, // 6ws
	{ 0x20737737U, GearSlot::Scepter, NonGearKind::None, true, false, 66u, 2u, 3u }, // 7ws
	{ 0x20737738U, GearSlot::Staff, NonGearKind::None, true, true, 25u, 2u, 4u }, // 8ws
	{ 0x20737739U, GearSlot::Scepter, NonGearKind::None, true, false, 25u, 2u, 3u }, // 9ws
	{ 0x20737774U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // tws
	{ 0x20746237U, GearSlot::Axe, NonGearKind::None, true, true, 54u, 2u, 3u }, // 7bt
	{ 0x20746239U, GearSlot::Axe, NonGearKind::None, true, true, 25u, 2u, 3u }, // 9bt
	{ 0x20746268U, GearSlot::Boots, NonGearKind::None, true, false, 0u, 2u, 2u }, // hbt
	{ 0x2074626CU, GearSlot::Boots, NonGearKind::None, true, false, 0u, 2u, 2u }, // lbt
	{ 0x2074626DU, GearSlot::Boots, NonGearKind::None, true, false, 0u, 2u, 2u }, // mbt
	{ 0x20746274U, GearSlot::Boots, NonGearKind::None, true, false, 0u, 2u, 2u }, // tbt
	{ 0x20746276U, GearSlot::Boots, NonGearKind::None, true, false, 0u, 2u, 2u }, // vbt
	{ 0x20746562U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // bet
	{ 0x2074696BU, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 3u }, // kit
	{ 0x20746975U, GearSlot::Shield, NonGearKind::None, true, false, 54u, 2u, 3u }, // uit
	{ 0x20746978U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 3u }, // xit
	{ 0x20746C70U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // plt
	{ 0x20746C75U, GearSlot::Torso, NonGearKind::None, true, false, 59u, 2u, 3u }, // ult
	{ 0x20746C78U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xlt
	{ 0x20746D37U, GearSlot::ClubMace, NonGearKind::None, true, false, 52u, 1u, 3u }, // 7mt
	{ 0x20746D39U, GearSlot::ClubMace, NonGearKind::None, true, false, 25u, 1u, 3u }, // 9mt
	{ 0x20747073U, GearSlot::Spear, NonGearKind::None, true, true, 0u, 2u, 4u }, // spt
	{ 0x20747268U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // hrt
	{ 0x20747337U, GearSlot::Spear, NonGearKind::None, true, true, 62u, 2u, 4u }, // 7st
	{ 0x20747339U, GearSlot::Spear, NonGearKind::None, true, true, 25u, 2u, 4u }, // 9st
	{ 0x20747362U, GearSlot::Staff, NonGearKind::None, true, true, 0u, 1u, 4u }, // bst
	{ 0x20747363U, GearSlot::Staff, NonGearKind::None, true, true, 0u, 1u, 4u }, // cst
	{ 0x20747368U, GearSlot::Staff, NonGearKind::None, true, true, 0u, 1u, 4u }, // hst
	{ 0x2074736CU, GearSlot::Staff, NonGearKind::None, true, true, 0u, 1u, 4u }, // lst
	{ 0x2074736DU, GearSlot::ClubMace, NonGearKind::None, true, false, 0u, 1u, 3u }, // mst
	{ 0x20747373U, GearSlot::Staff, NonGearKind::None, true, true, 0u, 1u, 3u }, // sst
	{ 0x20747377U, GearSlot::Staff, NonGearKind::None, true, true, 0u, 2u, 4u }, // wst
	{ 0x20747473U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // stt
	{ 0x2075616DU, GearSlot::ClubMace, NonGearKind::None, true, true, 0u, 2u, 4u }, // mau
	{ 0x20756D61U, GearSlot::None, NonGearKind::Amulet, false, false, 0u, 1u, 1u }, // amu
	{ 0x20756F76U, GearSlot::Polearm, NonGearKind::None, true, true, 0u, 2u, 4u }, // vou
	{ 0x20757473U, GearSlot::Torso, NonGearKind::None, true, false, 0u, 2u, 3u }, // stu
	{ 0x20757475U, GearSlot::Torso, NonGearKind::None, true, false, 53u, 2u, 3u }, // utu
	{ 0x20757478U, GearSlot::Torso, NonGearKind::None, true, false, 25u, 2u, 3u }, // xtu
	{ 0x2076616AU, GearSlot::Javelin, NonGearKind::None, true, false, 0u, 1u, 3u }, // jav
	{ 0x20766C67U, GearSlot::Javelin, NonGearKind::None, true, false, 0u, 1u, 4u }, // glv
	{ 0x20767161U, GearSlot::Arrows, NonGearKind::None, true, false, 1u, 1u, 3u }, // aqv
	{ 0x20767163U, GearSlot::Bolts, NonGearKind::None, true, false, 1u, 1u, 3u }, // cqv
	{ 0x2076756CU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 2u }, // luv
	{ 0x2077616AU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // jaw
	{ 0x20776237U, GearSlot::Wand, NonGearKind::None, true, false, 56u, 1u, 2u }, // 7bw
	{ 0x20776239U, GearSlot::Wand, NonGearKind::None, true, false, 25u, 1u, 2u }, // 9bw
	{ 0x20776263U, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 3u }, // cbw
	{ 0x20776268U, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 3u }, // hbw
	{ 0x2077626CU, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 4u }, // lbw
	{ 0x20776273U, GearSlot::Bow, NonGearKind::None, true, true, 0u, 2u, 3u }, // sbw
	{ 0x2077656AU, GearSlot::None, NonGearKind::Jewel, false, false, 0u, 1u, 1u }, // jew
	{ 0x20776737U, GearSlot::Wand, NonGearKind::None, true, false, 64u, 1u, 2u }, // 7gw
	{ 0x20776739U, GearSlot::Wand, NonGearKind::None, true, false, 25u, 1u, 2u }, // 9gw
	{ 0x20776A63U, GearSlot::None, NonGearKind::Jewel, false, false, 0u, 1u, 1u }, // cjw
	{ 0x20776C36U, GearSlot::Bow, NonGearKind::None, true, true, 63u, 2u, 4u }, // 6lw
	{ 0x20776C37U, GearSlot::Claw, NonGearKind::None, true, false, 58u, 1u, 3u }, // 7lw
	{ 0x20776C38U, GearSlot::Bow, NonGearKind::None, true, true, 25u, 2u, 4u }, // 8lw
	{ 0x20776C39U, GearSlot::Claw, NonGearKind::None, true, false, 33u, 1u, 3u }, // 9lw
	{ 0x20776C63U, GearSlot::Claw, NonGearKind::None, true, false, 0u, 1u, 3u }, // clw
	{ 0x20776F74U, GearSlot::Shield, NonGearKind::None, true, false, 0u, 2u, 3u }, // tow
	{ 0x20776F75U, GearSlot::Shield, NonGearKind::None, true, false, 59u, 2u, 3u }, // uow
	{ 0x20776F78U, GearSlot::Shield, NonGearKind::None, true, false, 25u, 2u, 3u }, // xow
	{ 0x20777336U, GearSlot::Bow, NonGearKind::None, true, true, 60u, 2u, 3u }, // 6sw
	{ 0x20777338U, GearSlot::Bow, NonGearKind::None, true, true, 25u, 2u, 3u }, // 8sw
	{ 0x20777362U, GearSlot::Sword, NonGearKind::None, true, true, 0u, 1u, 4u }, // bsw
	{ 0x20777437U, GearSlot::Claw, NonGearKind::None, true, false, 60u, 1u, 3u }, // 7tw
	{ 0x20777439U, GearSlot::Claw, NonGearKind::None, true, false, 37u, 1u, 3u }, // 9tw
	{ 0x20777937U, GearSlot::Wand, NonGearKind::None, true, false, 48u, 1u, 2u }, // 7yw
	{ 0x20777939U, GearSlot::Wand, NonGearKind::None, true, false, 25u, 1u, 2u }, // 9yw
	{ 0x20786132U, GearSlot::Axe, NonGearKind::None, true, false, 0u, 2u, 3u }, // 2ax
	{ 0x20786137U, GearSlot::Axe, NonGearKind::None, true, false, 45u, 2u, 3u }, // 7ax
	{ 0x20786139U, GearSlot::Axe, NonGearKind::None, true, false, 22u, 2u, 3u }, // 9ax
	{ 0x20786162U, GearSlot::Axe, NonGearKind::None, true, true, 0u, 2u, 3u }, // bax
	{ 0x20786167U, GearSlot::Axe, NonGearKind::None, true, true, 0u, 2u, 4u }, // gax
	{ 0x20786168U, GearSlot::Axe, NonGearKind::None, true, false, 0u, 1u, 3u }, // hax
	{ 0x2078616CU, GearSlot::Axe, NonGearKind::None, true, true, 0u, 2u, 3u }, // lax
	{ 0x20786170U, GearSlot::Polearm, NonGearKind::None, true, true, 0u, 2u, 4u }, // pax
	{ 0x20786174U, GearSlot::Axe, NonGearKind::None, true, false, 0u, 1u, 2u }, // tax
	{ 0x20786177U, GearSlot::Axe, NonGearKind::None, true, false, 0u, 2u, 3u }, // wax
	{ 0x20786836U, GearSlot::Crossbow, NonGearKind::None, true, true, 56u, 2u, 4u }, // 6hx
	{ 0x20786838U, GearSlot::Crossbow, NonGearKind::None, true, true, 25u, 2u, 4u }, // 8hx
	{ 0x20786967U, GearSlot::Axe, NonGearKind::None, true, true, 0u, 2u, 3u }, // gix
	{ 0x20786C36U, GearSlot::Crossbow, NonGearKind::None, true, true, 42u, 2u, 3u }, // 6lx
	{ 0x20786C38U, GearSlot::Crossbow, NonGearKind::None, true, true, 22u, 2u, 3u }, // 8lx
	{ 0x20786C65U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // elx
	{ 0x20786D36U, GearSlot::Crossbow, NonGearKind::None, true, true, 50u, 2u, 3u }, // 6mx
	{ 0x20786D38U, GearSlot::Crossbow, NonGearKind::None, true, true, 25u, 2u, 3u }, // 8mx
	{ 0x20786F62U, GearSlot::None, NonGearKind::None, true, false, 0u, 2u, 2u }, // box
	{ 0x20787236U, GearSlot::Crossbow, NonGearKind::None, true, true, 63u, 2u, 3u }, // 6rx
	{ 0x20787238U, GearSlot::Crossbow, NonGearKind::None, true, true, 25u, 2u, 3u }, // 8rx
	{ 0x20787462U, GearSlot::Axe, NonGearKind::None, true, true, 0u, 2u, 3u }, // btx
	{ 0x20796373U, GearSlot::Polearm, NonGearKind::None, true, true, 0u, 2u, 4u }, // scy
	{ 0x20796562U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // bey
	{ 0x2079656BU, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // key
	{ 0x20796571U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // qey
	{ 0x207A6373U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // scz
	{ 0x207A7262U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // brz
	{ 0x207A7965U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // eyz
	{ 0x207A7978U, GearSlot::None, NonGearKind::None, true, false, 0u, 1u, 1u }, // xyz
};

// Index == D2RL::Items::ItemInfo::qualityRecordId for a quality==Set item.
// Empty string means "row has no usable set/item text" (skip grouping).
inline constexpr const char* kSetGroupByRow[455] = {
	"Civerb's Vestments",
	"Civerb's Vestments",
	"Civerb's Vestments",
	"Hsarus' Defense",
	"Hsarus' Defense",
	"Hsarus' Defense",
	"Cleglaw's Brace",
	"Cleglaw's Brace",
	"Cleglaw's Brace",
	"Iratha's Finery",
	"Iratha's Finery",
	"Iratha's Finery",
	"Iratha's Finery",
	"Isenhart's Armory",
	"Isenhart's Armory",
	"Isenhart's Armory",
	"Isenhart's Armory",
	"Vidala's Rig",
	"Vidala's Rig",
	"Vidala's Rig",
	"Vidala's Rig",
	"Milabrega's Regalia",
	"Milabrega's Regalia",
	"Milabrega's Regalia",
	"Milabrega's Regalia",
	"Cathan's Traps",
	"Cathan's Traps",
	"Cathan's Traps",
	"Cathan's Traps",
	"Cathan's Traps",
	"Tancred's Battlegear",
	"Tancred's Battlegear",
	"Tancred's Battlegear",
	"Tancred's Battlegear",
	"Tancred's Battlegear",
	"Sigon's Complete Steel",
	"Sigon's Complete Steel",
	"Sigon's Complete Steel",
	"Sigon's Complete Steel",
	"Sigon's Complete Steel",
	"Sigon's Complete Steel",
	"Infernal Tools",
	"Infernal Tools",
	"Infernal Tools",
	"Berserker's Arsenal",
	"Berserker's Arsenal",
	"Berserker's Arsenal",
	"Death's Disguise",
	"Death's Disguise",
	"Death's Disguise",
	"Angelic Raiment",
	"Angelic Raiment",
	"Angelic Raiment",
	"Angelic Raiment",
	"Arctic Gear",
	"Arctic Gear",
	"Arctic Gear",
	"Arctic Gear",
	"Arcanna's Tricks",
	"Arcanna's Tricks",
	"Arcanna's Tricks",
	"Arcanna's Tricks",
	"Natalya's Odium",
	"Natalya's Odium",
	"Natalya's Odium",
	"Natalya's Odium",
	"Aldur's Watchtower",
	"Aldur's Watchtower",
	"Aldur's Watchtower",
	"Aldur's Watchtower",
	"Immortal King",
	"Immortal King",
	"Immortal King",
	"Immortal King",
	"Immortal King",
	"Immortal King",
	"Tal Rasha's Wrappings",
	"Tal Rasha's Wrappings",
	"Tal Rasha's Wrappings",
	"Tal Rasha's Wrappings",
	"Tal Rasha's Wrappings",
	"Griswold's Legacy",
	"Griswold's Legacy",
	"Griswold's Legacy",
	"Griswold's Legacy",
	"Trang-Oul's Avatar",
	"Trang-Oul's Avatar",
	"Trang-Oul's Avatar",
	"Trang-Oul's Avatar",
	"Trang-Oul's Avatar",
	"M'avina's Battle Hymn",
	"M'avina's Battle Hymn",
	"M'avina's Battle Hymn",
	"M'avina's Battle Hymn",
	"M'avina's Battle Hymn",
	"The Disciple",
	"The Disciple",
	"The Disciple",
	"The Disciple",
	"The Disciple",
	"Heaven's Brethren",
	"Heaven's Brethren",
	"Heaven's Brethren",
	"Heaven's Brethren",
	"Orphan's Call",
	"Orphan's Call",
	"Orphan's Call",
	"Orphan's Call",
	"Hwanin's Majesty",
	"Hwanin's Majesty",
	"Hwanin's Majesty",
	"Hwanin's Majesty",
	"Sazabi's Grand Tribute",
	"Sazabi's Grand Tribute",
	"Sazabi's Grand Tribute",
	"Bul-Kathos' Children",
	"Bul-Kathos' Children",
	"Cow King's Leathers",
	"Cow King's Leathers",
	"Cow King's Leathers",
	"Naj's Ancient Vestige",
	"Naj's Ancient Vestige",
	"Naj's Ancient Vestige",
	"Sander's Folly",
	"Sander's Folly",
	"Sander's Folly",
	"Sander's Folly",
	"Warlord's Glory",
	"Warlord's Glory",
	"Warlord's Glory",
	"Warlord's Glory",
	"Warlord's Glory",
	"Bane's Garments",
	"Bane's Garments",
	"Bane's Garments",
	"Horazon's Splendor",
	"Horazon's Splendor",
	"Horazon's Splendor",
	"Horazon's Splendor",
	"Horazon's Splendor",
	"Midnight Calling",
	"Midnight Calling",
	"Midnight Calling",
	"Midnight Calling",
	"Nature's Grove",
	"Nature's Grove",
	"Nature's Grove",
	"Nature's Grove",
	"Grimlock's Grave",
	"Grimlock's Grave",
	"Grimlock's Grave",
	"Grimlock's Grave",
	"Corgina's Element",
	"Corgina's Element",
	"Corgina's Element",
	"Corgina's Element",
	"Sheena's Grace",
	"Sheena's Grace",
	"Sheena's Grace",
	"Sheena's Grace",
	"Talonrage's Fury",
	"Talonrage's Fury",
	"Talonrage's Fury",
	"Talonrage's Fury",
	"Greyhawk's Mantle",
	"Greyhawk's Mantle",
	"Greyhawk's Mantle",
	"Greyhawk's Mantle",
	"Silent Runnings",
	"Silent Runnings",
	"Silent Runnings",
	"Silent Runnings",
	"Silent Runnings",
	"Snowmane's Jewelry",
	"Snowmane's Jewelry",
	"Snowmane's Jewelry",
	"Snowmane's Jewelry",
	"Four Seasons",
	"Four Seasons",
	"Four Seasons",
	"Four Seasons",
	"Forgotten Treasures",
	"Forgotten Treasures",
	"Forgotten Treasures",
	"Forgotten Treasures",
	"Forgotten Treasures",
	"Forgotten Treasures",
	"Insight of Brother Laz",
	"Insight of Brother Laz",
	"Insight of Brother Laz",
	"Insight of Brother Laz",
	"Insight of Brother Laz",
	"Hades' Underworld",
	"Hades' Underworld",
	"Hades' Underworld",
	"Hades' Underworld",
	"Hades' Underworld",
	"Darque's Cabal",
	"Darque's Cabal",
	"Darque's Cabal",
	"Darque's Cabal",
	"Red Havoc's Challenge",
	"Red Havoc's Challenge",
	"Red Havoc's Challenge",
	"Red Havoc's Challenge",
	"Mishy's Avatar",
	"Mishy's Avatar",
	"Mishy's Avatar",
	"Mishy's Avatar",
	"Mishy's Avatar",
	"Mishy's Avatar",
	"Joel's Sanctuary",
	"Joel's Sanctuary",
	"Joel's Sanctuary",
	"Joel's Sanctuary",
	"JBouley's Scion",
	"JBouley's Scion",
	"JBouley's Scion",
	"JBouley's Scion",
	"Forsaken Divinity",
	"Forsaken Divinity",
	"Forsaken Divinity",
	"Forsaken Divinity",
	"Volf's Undead Legion",
	"Volf's Undead Legion",
	"Volf's Undead Legion",
	"Volf's Undead Legion",
	"Legacy of Vashna",
	"Legacy of Vashna",
	"Legacy of Vashna",
	"Salander's Tirade",
	"Salander's Tirade",
	"Salander's Tirade",
	"Jerik's Dragon Armor",
	"Jerik's Dragon Armor",
	"Jerik's Dragon Armor",
	"Onyx's Primal Rage",
	"Onyx's Primal Rage",
	"Onyx's Primal Rage",
	"Onyx's Primal Rage",
	"Onyx's Primal Rage",
	"Brother Laz' Calling",
	"Brother Laz' Calling",
	"Brother Laz' Calling",
	"Brother Laz' Calling",
	"Brother Laz' Calling",
	"The Mysterious Spin",
	"The Mysterious Spin",
	"The Mysterious Spin",
	"The Mysterious Spin",
	"The Mysterious Spin",
	"Darkmage's Astral Projection",
	"Darkmage's Astral Projection",
	"Darkmage's Astral Projection",
	"Darkmage's Astral Projection",
	"Darkmage's Astral Projection",
	"Phrozen Heart's Mysticism",
	"Phrozen Heart's Mysticism",
	"Phrozen Heart's Mysticism",
	"Phrozen Heart's Mysticism",
	"Phrozen Heart's Mysticism",
	"Chaos Energy",
	"Chaos Energy",
	"Chaos Energy",
	"Chaos Energy",
	"Chaos Energy",
	"Aiel Shieldmaiden",
	"Aiel Shieldmaiden",
	"Aiel Shieldmaiden",
	"Aiel Shieldmaiden",
	"Aiel Shieldmaiden",
	"Myhrginoc's Warbreeder",
	"Myhrginoc's Warbreeder",
	"Myhrginoc's Warbreeder",
	"Myhrginoc's Warbreeder",
	"Nefarious Ways",
	"Nefarious Ways",
	"Nefarious Ways",
	"Nefarious Ways",
	"Kraj's Memorial",
	"Kraj's Memorial",
	"Kraj's Memorial",
	"Kraj's Memorial",
	"The Darkest Weaves",
	"The Darkest Weaves",
	"The Darkest Weaves",
	"Blood Raven's Despair",
	"Blood Raven's Despair",
	"Blood Raven's Despair",
	"Narrow Path Between Light and Darkness",
	"Narrow Path Between Light and Darkness",
	"Narrow Path Between Light and Darkness",
	"Narrow Path Between Light and Darkness",
	"Treasure Hunter",
	"Treasure Hunter",
	"Treasure Hunter",
	"Treasure Hunter",
	"Treasure Hunter",
	"Treasure Hunter",
	"Narrow Path Between Light and Darkness",
	"Yohann's Savant",
	"Yohann's Savant",
	"Yohann's Savant",
	"Yohann's Savant",
	"Zhoulomcrist's Dread",
	"Zhoulomcrist's Dread",
	"Zhoulomcrist's Dread",
	"Zhoulomcrist's Dread",
	"The Warlord of Blood",
	"The Warlord of Blood",
	"The Warlord of Blood",
	"The Warlord of Blood",
	"The Warlord of Blood",
	"The Warlord of Blood",
	"Darque Necromancy",
	"Darque Necromancy",
	"Darque Necromancy",
	"Darque Necromancy",
	"Darque Necromancy",
	"Marhawkman's Disguise",
	"Marhawkman's Disguise",
	"Marhawkman's Disguise",
	"Marhawkman's Disguise",
	"Sines Psionics",
	"Sines Psionics",
	"Sines Psionics",
	"Sines Psionics",
	"Bogrot's Magic",
	"Bogrot's Magic",
	"Bogrot's Magic",
	"Cedric's Jinx",
	"Cedric's Jinx",
	"Cedric's Jinx",
	"Dragon Reborn",
	"Dragon Reborn",
	"Dragon Reborn",
	"Dragon Reborn",
	"Dragon Reborn",
	"Dragon Reborn",
	"Soldier's Cairn",
	"Soldier's Cairn",
	"Soldier's Cairn",
	"Gweibret's Rule",
	"Gweibret's Rule",
	"Gweibret's Rule",
	"Gweibret's Rule",
	"Gweibret's Rule",
	"Haunted Asylum",
	"Haunted Asylum",
	"Haunted Asylum",
	"Haunted Asylum",
	"Haunted Asylum",
	"Alyssa's Archery",
	"Alyssa's Archery",
	"Alyssa's Archery",
	"Forbidden Lore",
	"Forbidden Lore",
	"Forbidden Lore",
	"Forbidden Lore",
	"Hannibal's Demise",
	"Hannibal's Demise",
	"Hannibal's Demise",
	"Hannibal's Demise",
	"Wrath of Vengeance",
	"Wrath of Vengeance",
	"Wrath of Vengeance",
	"Wrath of Vengeance",
	"Wrath of Vengeance",
	"Servitude or Rebellion",
	"Servitude or Rebellion",
	"Servitude or Rebellion",
	"Kaldorn's Majesty",
	"Kaldorn's Majesty",
	"Kaldorn's Majesty",
	"Kaldorn's Majesty",
	"Kaldorn's Majesty",
	"Warlock's Exploration",
	"Warlock's Exploration",
	"Warlock's Exploration",
	"Warlock's Exploration",
	"Warlock's Exploration",
	"Amaunator's Peace",
	"Amaunator's Peace",
	"Amaunator's Peace",
	"Amaunator's Peace",
	"Amaunator's Peace",
	"Amaunator's Peace",
	"Tika's Request",
	"Tika's Request",
	"Tika's Request",
	"Tika's Request",
	"Tika's Request",
	"Corthala Family Heirlooms",
	"Corthala Family Heirlooms",
	"Corthala Family Heirlooms",
	"Corthala Family Heirlooms",
	"Corthala Family Heirlooms",
	"Knight's Gallantry",
	"Knight's Gallantry",
	"Knight's Gallantry",
	"Knight's Gallantry",
	"Knight's Gallantry",
	"Stormsigil Vestments",
	"Stormsigil Vestments",
	"Stormsigil Vestments",
	"Stormsigil Vestments",
	"Kai Lord's Valiance",
	"Kai Lord's Valiance",
	"Kai Lord's Valiance",
	"Kai Lord's Valiance",
	"Kai Lord's Valiance",
	"Path of Bravery",
	"Path of Bravery",
	"Path of Bravery",
	"Path of Bravery",
	"Path of Bravery",
	"Path of Bravery",
	"Terror of the Deep",
	"Terror of the Deep",
	"Terror of the Deep",
	"Terror of the Deep",
	"Celestial Hierarchy",
	"Celestial Hierarchy",
	"Celestial Hierarchy",
	"Celestial Hierarchy",
	"Celestial Hierarchy",
	"Celestial Hierarchy",
	"Lords of Hell",
	"Lords of Hell",
	"Lords of Hell",
	"Lords of Hell",
	"Corruption Coils",
	"Corruption Coils",
	"Corruption Coils",
	"Maadi's Paradox",
	"Maadi's Paradox",
	"Maadi's Paradox",
	"Maadi's Paradox",
	"Maadi's Paradox",
	"The Raven's Nest",
	"The Raven's Nest",
	"The Raven's Nest",
	"The Raven's Nest",
	"The Raven's Nest",
	"Tools of Vindication",
	"Tools of Vindication",
	"Tools of Vindication",
	"Panda's Polar Adventure",
	"Panda's Polar Adventure",
	"Panda's Polar Adventure",
	"Panda's Polar Adventure",
	"Panda's Polar Adventure",
	"Deus et Diabolus",
	"Deus et Diabolus",
	"Deus et Diabolus",
};

// setitems.txt / uniqueitems.txt 'lvl req', index == qualityRecordId (see
// TrueReqLevel() in item_classification.cpp). 0 for an out-of-data-range row.
inline constexpr uint16_t kSetLevelReqByRow[455] = {
	9, 9, 9, 3, 3, 3, 4, 4, 4, 15, 15, 15, 15, 8, 8, 8, 8, 14, 14, 14, 14, 17, 17, 17, 17, 11, 11, 11, 11, 11, 20, 20, 20, 20, 20, 6, 6, 6, 6, 6, 6, 5, 5, 5, 3, 3, 3, 6, 6, 6, 12, 12, 12, 12, 2, 2, 2, 2, 15, 15, 15, 15, 59, 79, 73, 25, 36, 76, 42, 45, 47, 76, 29, 30, 31, 76, 53, 67, 65, 71, 66, 69, 45, 80, 68, 80, 49, 54, 45, 70, 59, 70, 32, 45, 70, 30, 60, 29, 43, 60, 68, 81, 50, 69, 34, 42, 37, 29, 45, 30, 35, 28, 73, 67, 43, 85, 85, 25, 18, 13, 78, 71, 28, 15, 15, 15, 15, 1, 1, 1, 1, 1, 8, 8, 8, 62, 55, 48, 67, 57, 12, 10, 5, 9, 12, 18, 7, 15, 16, 15, 11, 9, 15, 21, 14, 9, 15, 12, 14, 10, 18, 22, 20, 21, 19, 14, 15, 13, 18, 14, 23, 14, 15, 22, 22, 22, 22, 25, 22, 21, 19, 15, 15, 10, 10, 15, 10, 33, 38, 40, 29, 36, 34, 36, 39, 32, 41, 40, 43, 42, 41, 35, 33, 39, 32, 32, 35, 40, 38, 33, 40, 33, 33, 33, 33, 35, 32, 35, 25, 45, 45, 45, 45, 38, 37, 39, 42, 30, 37, 33, 37, 35, 34, 37, 37, 40, 77, 69, 75, 80, 79, 81, 83, 69, 74, 71, 52, 51, 46, 59, 50, 73, 70, 74, 69, 80, 77, 71, 74, 73, 75, 81, 69, 83, 74, 77, 80, 73, 75, 71, 74, 80, 77, 70, 79, 80, 75, 76, 70, 80, 83, 79, 77, 76, 77, 78, 73, 77, 74, 55, 51, 51, 67, 25, 42, 42, 7, 61, 35, 65, 39, 33, 38, 23, 31, 60, 65, 50, 29, 31, 45, 47, 68, 77, 30, 63, 29, 50, 35, 69, 72, 36, 60, 50, 70, 75, 40, 34, 31, 48, 36, 75, 43, 41, 44, 67, 67, 67, 21, 35, 55, 73, 40, 69, 46, 70, 50, 44, 42, 47, 65, 72, 47, 44, 60, 46, 63, 16, 32, 48, 62, 38, 45, 50, 64, 67, 71, 39, 51, 70, 43, 63, 70, 74, 55, 58, 55, 60, 64, 75, 66, 70, 47, 65, 33, 22, 30, 60, 72, 40, 75, 48, 73, 79, 81, 44, 56, 77, 68, 51, 60, 30, 56, 30, 52, 30, 74, 83, 77, 58, 63, 73, 40, 75, 77, 76, 50, 74, 77, 39, 44, 81, 45, 36, 70, 61, 49, 80, 77, 74, 51, 45, 45, 45, 47, 65, 47, 64, 81, 10, 10, 45, 60, 80, 20, 60, 40, 50, 12, 80, 35, 60, 80, 76, 78,
};

inline constexpr uint16_t kUniqueLevelReqByRow[1483] = {
	5, 9, 15, 21, 27, 8, 14, 19, 25, 29, 5, 14, 20, 28, 3, 5, 18, 30, 5, 9, 15, 21, 27, 24, 29, 2, 7, 10, 13, 13, 17, 22, 25, 12, 19, 23, 26, 13, 16, 4, 11, 19, 20, 8, 12, 17, 22, 27, 8, 14, 19, 25, 28, 29, 5, 9, 18, 21, 28, 7, 13, 15, 15, 25, 26, 90, 28, 9, 18, 27, 28, 3, 15, 14, 17, 21, 25, 29, 20, 7, 12, 14, 16, 25, 15, 17, 20, 22, 26, 22, 29, 28, 28, 29, 2, 9, 13, 20, 15, 17, 19, 26, 5, 9, 15, 23, 45, 5, 9, 15, 22, 29, 7, 12, 20, 27, 29, 10, 15, 25, 30, 15, 29, 0, 0, 0, 0, 0, 0, 36, 39, 43, 45, 48, 35, 39, 41, 42, 45, 33, 35, 36, 41, 29, 37, 38, 55, 38, 39, 42, 45, 43, 48, 45, 30, 31, 33, 30, 35, 39, 41, 35, 42, 43, 44, 45, 46, 48, 32, 27, 80, 41, 31, 33, 30, 37, 39, 41, 42, 42, 43, 44, 75, 28, 45, 45, 39, 41, 28, 31, 33, 36, 39, 41, 43, 55, 32, 36, 41, 49, 0, 28, 31, 35, 38, 44, 49, 41, 41, 28, 29, 31, 33, 35, 37, 38, 41, 42, 45, 48, 51, 53, 55, 51, 28, 31, 35, 45, 44, 50, 41, 35, 29, 55, 36, 42, 47, 29, 32, 36, 42, 47, 29, 32, 36, 50, 47, 0, 62, 73, 85, 85, 61, 73, 80, 70, 75, 85, 87, 58, 69, 81, 61, 95, 75, 69, 85, 0, 58, 50, 65, 68, 67, 60, 45, 45, 65, 55, 0, 60, 42, 42, 42, 42, 42, 50, 42, 55, 47, 61, 65, 40, 75, 80, 67, 0, 75, 62, 84, 70, 69, 85, 65, 0, 68, 0, 64, 75, 66, 75, 63, 84, 74, 72, 70, 85, 64, 65, 0, 76, 82, 80, 82, 81, 81, 75, 75, 67, 76, 77, 64, 65, 75, 70, 78, 80, 76, 83, 65, 0, 68, 80, 69, 67, 82, 83, 0, 80, 66, 76, 74, 83, 0, 71, 81, 79, 68, 77, 85, 55, 82, 77, 0, 80, 76, 70, 74, 75, 81, 64, 76, 75, 0, 80, 51, 81, 63, 0, 60, 65, 75, 75, 81, 63, 68, 85, 76, 80, 66, 85, 75, 70, 49, 49, 49, 49, 49, 49, 49, 49, 75, 75, 75, 75, 75, 75, 75, 0, 80, 73, 78, 52, 61, 67, 67, 50, 50, 54, 71, 50, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 0, 15, 6, 22, 10, 16, 14, 30, 23, 35, 30, 44, 38, 45, 49, 56, 50, 53, 60, 65, 57, 68, 71, 64, 75, 80, 84, 21, 8, 23, 14, 27, 23, 33, 28, 37, 32, 45, 40, 51, 44, 47, 54, 59, 50, 52, 58, 75, 77, 69, 73, 79, 84, 12, 5, 15, 8, 16, 15, 20, 15, 15, 65, 13, 20, 10, 15, 18, 25, 30, 37, 40, 41, 35, 43, 32, 37, 39, 47, 42, 45, 50, 52, 47, 49, 44, 60, 49, 64, 60, 65, 57, 70, 74, 85, 87, 70, 15, 12, 15, 10, 25, 20, 30, 27, 15, 18, 27, 20, 48, 38, 52, 46, 81, 49, 59, 51, 55, 75, 70, 56, 65, 54, 75, 60, 77, 80, 84, 75, 87, 20, 7, 25, 9, 50, 40, 59, 46, 70, 82, 35, 30, 60, 50, 39, 30, 46, 33, 59, 48, 65, 55, 87, 20, 13, 29, 20, 34, 29, 55, 45, 45, 49, 57, 53, 75, 80, 77, 19, 8, 25, 22, 68, 29, 55, 39, 58, 48, 46, 54, 81, 77, 86, 20, 6, 20, 14, 18, 15, 21, 17, 31, 29, 37, 33, 40, 46, 50, 30, 70, 74, 62, 80, 85, 18, 85, 23, 15, 30, 17, 32, 30, 38, 30, 45, 35, 46, 39, 49, 44, 54, 47, 57, 60, 60, 65, 58, 85, 90, 72, 86, 20, 10, 23, 14, 26, 19, 29, 24, 38, 28, 35, 32, 44, 38, 50, 42, 50, 45, 53, 49, 51, 57, 47, 53, 70, 75, 70, 75, 79, 87, 77, 86, 20, 6, 29, 12, 80, 32, 15, 36, 23, 40, 29, 55, 36, 50, 39, 56, 42, 60, 47, 65, 50, 67, 49, 69, 55, 80, 79, 76, 85, 25, 9, 30, 12, 33, 26, 35, 32, 43, 39, 42, 50, 45, 55, 49, 68, 50, 73, 65, 79, 70, 89, 7, 22, 85, 21, 87, 28, 25, 29, 15, 32, 26, 35, 29, 37, 86, 44, 35, 90, 37, 49, 35, 85, 42, 47, 45, 54, 48, 56, 50, 59, 53, 65, 60, 89, 80, 72, 60, 74, 64, 77, 67, 79, 85, 80, 6, 35, 65, 25, 12, 25, 18, 32, 24, 35, 30, 48, 38, 53, 43, 56, 47, 61, 54, 67, 55, 74, 64, 79, 85, 6, 35, 65, 21, 6, 3, 23, 15, 11, 27, 21, 14, 33, 21, 15, 37, 32, 29, 54, 37, 31, 47, 35, 35, 55, 35, 29, 65, 50, 41, 85, 54, 45, 70, 55, 61, 70, 77, 85, 20, 10, 6, 30, 27, 16, 50, 35, 56, 50, 37, 77, 85, 11, 7, 4, 80, 15, 12, 80, 30, 55, 39, 37, 80, 75, 80, 11, 6, 13, 8, 15, 10, 17, 12, 19, 14, 21, 16, 23, 18, 25, 40, 26, 22, 28, 35, 25, 21, 30, 30, 32, 28, 34, 30, 36, 30, 32, 41, 42, 34, 43, 36, 44, 38, 41, 45, 47, 50, 44, 55, 47, 51, 50, 55, 57, 52, 59, 55, 60, 56, 69, 55, 62, 58, 63, 59, 65, 67, 60, 69, 60, 73, 73, 65, 80, 67, 68, 86, 70, 80, 78, 80, 75, 82, 85, 75, 84, 75, 85, 8, 5, 16, 13, 20, 16, 23, 20, 27, 24, 30, 28, 90, 32, 28, 75, 36, 33, 36, 40, 39, 43, 43, 46, 85, 53, 53, 56, 44, 48, 46, 49, 55, 81, 60, 80, 66, 84, 77, 81, 30, 25, 18, 45, 40, 39, 82, 60, 86, 83, 8, 5, 14, 11, 19, 16, 25, 24, 22, 18, 24, 21, 25, 45, 31, 29, 37, 34, 39, 35, 42, 39, 48, 44, 53, 55, 55, 59, 48, 45, 46, 41, 80, 80, 67, 70, 75, 78, 82, 81, 86, 80, 65, 11, 8, 18, 14, 20, 25, 30, 27, 70, 31, 37, 34, 43, 39, 48, 70, 54, 50, 60, 55, 64, 57, 70, 85, 84, 83, 87, 10, 7, 17, 13, 23, 65, 30, 26, 34, 46, 40, 35, 45, 38, 48, 43, 50, 81, 60, 55, 70, 80, 75, 65, 74, 86, 65, 8, 5, 15, 12, 25, 21, 30, 28, 32, 60, 39, 36, 41, 37, 45, 40, 48, 46, 54, 50, 60, 64, 85, 78, 80, 85, 80, 20, 14, 14, 30, 24, 90, 40, 90, 33, 45, 40, 73, 87, 81, 25, 18, 20, 60, 45, 80, 15, 14, 20, 20, 35, 33, 50, 73, 80, 70, 5, 6, 10, 10, 25, 15, 20, 15, 20, 30, 40, 30, 40, 27, 40, 30, 50, 50, 39, 80, 81, 65, 72, 68, 73, 78, 84, 8, 4, 12, 9, 19, 15, 24, 20, 30, 26, 35, 29, 39, 33, 43, 36, 47, 40, 52, 55, 49, 75, 57, 87, 75, 75, 86, 8, 5, 17, 10, 22, 15, 20, 27, 25, 32, 37, 30, 41, 34, 45, 39, 50, 46, 65, 55, 72, 68, 70, 75, 81, 70, 88, 12, 6, 11, 18, 23, 17, 27, 22, 31, 26, 35, 30, 40, 33, 43, 36, 46, 39, 70, 55, 65, 70, 78, 90, 81, 87, 12, 4, 16, 8, 21, 14, 24, 19, 30, 25, 29, 34, 42, 40, 37, 43, 46, 40, 65, 81, 70, 59, 72, 75, 86, 70, 12, 7, 5, 12, 7, 5, 15, 83, 10, 20, 15, 15, 22, 24, 20, 25, 30, 33, 45, 30, 25, 40, 37, 30, 56, 41, 32, 45, 43, 36, 50, 30, 38, 40, 45, 40, 50, 50, 70, 54, 49, 64, 55, 81, 82, 55, 84, 85, 88, 75, 80, 83, 3, 11, 18, 24, 28, 24, 60, 50, 75, 73, 82, 85, 3, 6, 12, 27, 30, 38, 45, 81, 78, 85, 12, 80, 48, 60, 15, 55, 81, 6, 85, 58, 58, 49, 38, 43, 58, 58, 67, 71, 76, 85, 1, 10, 20, 30, 40, 50, 60, 75, 90, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 85, 85, 85, 85, 85, 85, 24, 32, 56, 80, 1, 1, 85, 85, 25, 50, 70, 80, 80, 1, 5, 10, 11, 14, 15, 25, 17, 24, 20, 27, 35, 37, 44, 45, 55, 55, 60, 50, 61, 70, 78, 69, 83, 83, 90, 90, 92, 85, 85, 15, 30, 80, 70, 90, 15, 30, 45, 70, 90, 89, 45,
};

// magicprefix.txt / magicsuffix.txt 'levelreq', index+1 == the affix's
// table-local ID within ItemInfo::prefixIds/suffixIds' *combined* ID space
// (see TrueReqLevel()'s AffixLevelReq helper for the decode).
inline constexpr uint16_t kMagicPrefixLevelReq[1117] = {
	1, 3, 6, 10, 15, 19, 24, 30, 38, 45, 51, 63, 69, 16, 41, 64, 1, 9, 37, 58, 18, 35, 49, 66, 77, 3, 3, 3, 3, 9, 9, 9, 9, 13, 13, 13, 13, 19, 19, 19, 19, 30, 30, 30, 30, 37, 37, 37, 37, 45, 45, 45, 45, 53, 60, 60, 60, 59, 65, 65, 65, 63, 69, 69, 69, 70, 77, 77, 77, 79, 82, 82, 82, 88, 88, 88, 88, 1, 5, 9, 12, 16, 20, 24, 34, 48, 59, 67, 72, 35, 57, 63, 78, 37, 51, 67, 80, 1, 11, 18, 26, 33, 41, 19, 1, 11, 18, 27, 37, 13, 1, 5, 9, 12, 16, 22, 27, 34, 40, 1, 6, 18, 36, 1, 7, 15, 26, 37, 15, 30, 45, 60, 75, 18, 6, 30, 9, 27, 50, 26, 52, 88, 1, 7, 15, 26, 37, 3, 15, 35, 55, 65, 1, 12, 24, 56, 1, 1, 22, 22, 40, 40, 60, 60, 1, 4, 10, 18, 25, 32, 48, 57, 69, 74, 75, 1, 59, 37, 51, 67, 75, 3, 14, 26, 37, 50, 67, 83, 50, 80, 80, 27, 47, 75, 67, 22, 42, 47, 27, 75, 67, 22, 42, 27, 58, 27, 47, 75, 67, 22, 42, 27, 47, 75, 67, 22, 42, 27, 47, 75, 67, 22, 42, 22, 42, 27, 47, 75, 67, 22, 42, 27, 47, 75, 67, 22, 42, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 15, 30, 45, 30, 50, 65, 75, 30, 50, 65, 75, 30, 50, 65, 75, 30, 50, 65, 80, 30, 50, 65, 75, 30, 50, 65, 75, 30, 50, 65, 75, 18, 26, 40, 60, 18, 26, 37, 51, 67, 18, 25, 36, 50, 66, 1, 7, 15, 26, 37, 3, 9, 26, 13, 41, 18, 1, 14, 9, 26, 13, 41, 18, 1, 13, 3, 9, 26, 13, 41, 18, 1, 14, 3, 9, 26, 13, 41, 18, 1, 14, 4, 13, 21, 31, 42, 6, 15, 25, 31, 41, 37, 48, 59, 12, 26, 2, 4, 10, 27, 15, 18, 39, 22, 29, 37, 2, 12, 21, 18, 2, 12, 16, 6, 5, 1, 4, 1, 3, 8, 12, 7, 25, 47, 1, 12, 30, 5, 10, 16, 21, 22, 42, 61, 80, 7, 12, 13, 7, 12, 18, 15, 40, 63, 15, 40, 63, 23, 44, 63, 77, 14, 27, 46, 68, 81, 7, 11, 20, 32, 43, 52, 1, 4, 9, 18, 30, 45, 55, 65, 1, 4, 9, 18, 30, 45, 55, 65, 6, 6, 12, 12, 12, 12, 18, 20, 24, 25, 30, 34, 40, 45, 48, 33, 55, 77, 33, 55, 77, 33, 55, 77, 33, 55, 77, 33, 55, 77, 33, 55, 77, 33, 55, 77, 33, 55, 77, 1, 10, 1, 12, 1, 35, 6, 20, 6, 16, 10, 45, 12, 25, 12, 27, 12, 30, 12, 55, 12, 29, 12, 24, 18, 40, 18, 55, 18, 50, 18, 65, 25, 74, 25, 75, 33, 74, 33, 74, 33, 74, 1, 12, 1, 13, 1, 9, 1, 35, 6, 45, 6, 26, 6, 15, 6, 15, 12, 24, 12, 30, 12, 30, 12, 24, 12, 55, 18, 65, 18, 65, 18, 35, 18, 65, 18, 40, 25, 65, 25, 55, 25, 42, 25, 60, 25, 75, 33, 85, 33, 85, 1, 20, 1, 12, 1, 35, 1, 26, 6, 20, 6, 24, 6, 24, 6, 45, 6, 20, 12, 24, 12, 24, 12, 37, 12, 36, 18, 35, 18, 41, 18, 28, 18, 45, 18, 38, 25, 44, 25, 75, 25, 40, 25, 55, 33, 80, 33, 73, 33, 85, 33, 70, 33, 85, 1, 35, 6, 18, 12, 55, 18, 43, 18, 45, 25, 45, 33, 74, 1, 10, 1, 9, 1, 15, 6, 18, 6, 45, 12, 24, 12, 55, 18, 36, 18, 36, 25, 55, 25, 68, 33, 63, 33, 53, 1, 13, 6, 21, 12, 36, 12, 55, 18, 65, 25, 50, 25, 75, 33, 70, 33, 85, 1, 10, 6, 12, 6, 45, 12, 24, 12, 24, 18, 65, 38, 18, 53, 18, 33, 25, 35, 25, 35, 25, 48, 33, 60, 33, 50, 33, 61, 33, 85, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 10, 27, 52, 10, 27, 52, 40, 50, 60, 10, 27, 52, 10, 27, 52, 40, 50, 60, 10, 27, 52, 10, 27, 52, 40, 50, 60, 10, 40, 60, 10, 27, 52, 10, 27, 52, 40, 50, 60, 10, 27, 52, 10, 27, 52, 40, 50, 60, 10, 27, 52, 61, 71, 81, 10, 27, 52, 10, 27, 52, 10, 27, 52, 10, 27, 52, 10, 27, 52, 40, 50, 60, 40, 50, 60, 40, 50, 60, 10, 45, 80, 10, 30, 60, 15, 15, 15, 15, 18, 18, 18, 18, 18, 18, 20, 20, 20, 20, 20, 20, 20, 45, 45, 45, 45, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 10, 50, 81, 1, 5, 10, 15, 20, 25, 29, 61, 81, 15, 35, 61, 3, 12, 21, 32, 46, 2, 7, 12, 18, 24, 31, 39, 26, 60, 88, 1, 5, 9, 12, 16, 20, 24, 29, 34, 48, 65, 75, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 5, 35, 61, 71, 81, 5, 35, 61, 81, 5, 35, 61, 81, 15, 35, 61, 81, 45, 65, 85, 45, 65, 85, 10, 20, 40, 55, 10, 20, 40, 55, 10, 20, 40, 55, 10, 20, 40, 55, 20, 40, 60, 1, 5, 10, 20, 30, 40, 61, 81, 85, 10, 35, 50, 81, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 42, 42, 42, 15, 30, 45, 15, 30, 45, 15, 30, 45, 27, 67, 22, 42, 75, 75, 75, 75, 75, 75, 20, 50, 80, 42, 42, 42, 30, 50, 65, 75, 1, 20, 50, 1, 20, 50, 1, 20, 50, 1, 30, 60, 1, 20, 60, 1, 20, 60, 1, 20, 60, 1, 20, 60, 1, 20, 60, 1, 20, 60,
};

inline constexpr uint16_t kMagicSuffixLevelReq[823] = {
	8, 27, 46, 1, 19, 49, 61, 1, 3, 5, 8, 10, 14, 18, 27, 37, 50, 65, 79, 34, 51, 68, 1, 10, 22, 35, 46, 62, 74, 3, 11, 30, 55, 1, 9, 18, 40, 68, 75, 83, 11, 29, 51, 73, 3, 37, 3, 9, 20, 37, 50, 75, 41, 55, 68, 81, 12, 29, 50, 3, 11, 18, 35, 50, 75, 30, 51, 62, 74, 11, 28, 49, 3, 11, 18, 26, 49, 68, 37, 57, 70, 80, 11, 28, 49, 3, 11, 18, 25, 33, 55, 72, 1, 33, 66, 7, 69, 39, 18, 13, 1, 4, 15, 43, 10, 35, 65, 26, 20, 45, 73, 4, 16, 48, 11, 40, 66, 27, 21, 46, 74, 38, 60, 84, 5, 13, 19, 26, 37, 18, 24, 33, 43, 5, 12, 19, 35, 18, 24, 33, 19, 31, 53, 72, 8, 17, 29, 47, 9, 16, 25, 38, 17, 50, 29, 54, 80, 8, 14, 29, 47, 9, 16, 25, 38, 16, 50, 29, 54, 80, 8, 14, 29, 47, 9, 16, 25, 38, 17, 50, 29, 54, 80, 1, 1, 1, 1, 1, 1, 1, 1, 1, 41, 71, 45, 45, 45, 45, 58, 75, 1, 8, 20, 35, 51, 64, 4, 16, 31, 48, 67, 9, 26, 38, 52, 5, 14, 28, 1, 1, 75, 1, 8, 19, 34, 50, 63, 3, 15, 30, 47, 66, 8, 25, 37, 51, 13, 25, 42, 1, 1, 1, 3, 9, 16, 23, 33, 5, 9, 16, 23, 33, 3, 7, 12, 21, 6, 18, 33, 1, 1, 1, 48, 35, 55, 75, 35, 55, 75, 35, 55, 75, 35, 55, 75, 18, 40, 64, 83, 1, 5, 11, 15, 18, 22, 30, 37, 3, 8, 20, 35, 51, 67, 6, 13, 26, 43, 60, 1, 37, 58, 7, 1, 1, 7, 30, 52, 12, 41, 30, 35, 1, 18, 31, 48, 75, 8, 16, 29, 57, 43, 3, 6, 13, 36, 3, 17, 26, 38, 35, 31, 15, 31, 10, 15, 26, 39, 35, 62, 29, 1, 45, 55, 48, 1, 4, 11, 12, 9, 12, 19, 31, 48, 19, 47, 75, 1, 12, 1, 18, 11, 1, 1, 12, 42, 3, 18, 18, 70, 12, 4, 1, 8, 3, 21, 6, 37, 6, 9, 28, 45, 13, 27, 41, 56, 20, 38, 57, 47, 19, 12, 33, 33, 33, 67, 1, 10, 20, 1, 10, 20, 1, 10, 20, 1, 35, 6, 45, 6, 23, 6, 24, 18, 18, 33, 63, 32, 61, 18, 48, 20, 24, 45, 18, 36, 24, 45, 30, 18, 40, 32, 65, 25, 25, 45, 29, 60, 38, 34, 85, 33, 85, 1, 20, 1, 18, 1, 35, 6, 20, 6, 24, 6, 45, 12, 30, 12, 30, 12, 37, 18, 35, 18, 41, 18, 36, 18, 55, 25, 44, 25, 75, 25, 44, 33, 80, 33, 48, 33, 48, 33, 85, 6, 24, 18, 45, 33, 60, 1, 18, 6, 24, 6, 45, 18, 36, 25, 55, 25, 55, 33, 63, 33, 53, 1, 18, 6, 21, 12, 36, 12, 37, 18, 38, 25, 44, 25, 50, 33, 70, 33, 65, 6, 45, 18, 38, 18, 53, 25, 48, 33, 50, 33, 61, 1, 46, 33, 33, 71, 1, 11, 1, 11, 1, 11, 18, 41, 6, 42, 6, 32, 18, 32, 32, 60, 48, 24, 45, 46, 18, 40, 32, 62, 29, 57, 34, 80, 48, 1, 35, 4, 24, 24, 30, 30, 24, 22, 32, 27, 30, 38, 30, 45, 55, 48, 51, 1, 18, 18, 36, 24, 48, 36, 60, 18, 30, 6, 18, 24, 24, 20, 6, 36, 30, 55, 39, 24, 55, 6, 36, 30, 36, 36, 36, 42, 42, 48, 36, 48, 48, 20, 20, 20, 20, 30, 30, 15, 45, 75, 10, 25, 40, 75, 10, 25, 40, 75, 10, 25, 40, 75, 10, 25, 40, 75, 14, 17, 39, 27, 29, 20, 60, 10, 30, 60, 10, 30, 60, 10, 30, 60, 10, 30, 60, 10, 30, 60, 10, 30, 60, 10, 30, 10, 30, 10, 30, 10, 30, 10, 10, 10, 10, 10, 10, 10, 10, 10, 18, 18, 9, 34, 66, 81, 81, 81, 81, 81, 81, 81, 81, 5, 35, 61, 81, 5, 35, 61, 81, 5, 35, 61, 81, 5, 35, 61, 81, 1, 35, 1, 35, 10, 20, 35, 45, 55, 61, 70, 81, 88, 15, 10, 30, 1, 35, 1, 35, 61, 61, 61, 61, 81, 81, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 7, 14, 12, 24, 18, 36, 36, 72, 81, 1, 30, 50, 1, 30, 50, 1, 30, 50, 1, 30, 50, 1, 20, 40, 1, 20, 40, 1, 20, 40, 1, 20, 40, 1, 20, 40, 1, 20, 40, 1, 20, 40, 1, 30, 50, 50, 50, 50, 80, 80, 80,
};

} // namespace RemoteStashSorting::Classification
