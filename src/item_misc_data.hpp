#pragma once

// GENERATED FILE -- do not edit by hand.
// Produced by tools/gen_misc_classification.py from a mod's
// data/global/excel/{itemtypes,misc}.txt. See that script for the rules.
//
// 226 misc item codes.

#include "sort_engine.hpp"
#include <cstdint>

namespace Quyen194ItemSorting::Classification {

struct MiscEntry {
	uint32_t code; // packed 4-byte item code, ascending, for binary search
	MiscKind kind;
	uint16_t order; // ItemFacts::miscOrder
	uint16_t grade; // ItemFacts::miscGrade
	uint8_t  width;
	uint8_t  height;
};

inline constexpr MiscEntry kMiscTable[226] = {
	{ 0x20303172U, MiscKind::Rune, 10u, 0u, 1u, 1u }, // r10
	{ 0x20303173U, MiscKind::Rune, 10u, 1u, 1u, 1u }, // s10
	{ 0x20303272U, MiscKind::Rune, 20u, 0u, 1u, 1u }, // r20
	{ 0x20303273U, MiscKind::Rune, 20u, 1u, 1u, 1u }, // s20
	{ 0x20303372U, MiscKind::Rune, 30u, 0u, 1u, 1u }, // r30
	{ 0x20303373U, MiscKind::Rune, 30u, 1u, 1u, 1u }, // s30
	{ 0x20313072U, MiscKind::Rune, 1u, 0u, 1u, 1u }, // r01
	{ 0x20313073U, MiscKind::Rune, 1u, 1u, 1u, 1u }, // s01
	{ 0x20313172U, MiscKind::Rune, 11u, 0u, 1u, 1u }, // r11
	{ 0x20313173U, MiscKind::Rune, 11u, 1u, 1u, 1u }, // s11
	{ 0x20313272U, MiscKind::Rune, 21u, 0u, 1u, 1u }, // r21
	{ 0x20313273U, MiscKind::Rune, 21u, 1u, 1u, 1u }, // s21
	{ 0x20313372U, MiscKind::Rune, 31u, 0u, 1u, 1u }, // r31
	{ 0x20313373U, MiscKind::Rune, 31u, 1u, 1u, 1u }, // s31
	{ 0x20316175U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // ua1
	{ 0x20316178U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // xa1
	{ 0x20316B70U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // pk1
	{ 0x20316D75U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // um1
	{ 0x20317068U, MiscKind::Potion, 1u, 1u, 1u, 1u }, // hp1
	{ 0x2031706DU, MiscKind::Potion, 2u, 1u, 1u, 1u }, // mp1
	{ 0x20317274U, MiscKind::Quest, 0u, 0u, 2u, 2u }, // tr1
	{ 0x20323072U, MiscKind::Rune, 2u, 0u, 1u, 1u }, // r02
	{ 0x20323073U, MiscKind::Rune, 2u, 1u, 1u, 1u }, // s02
	{ 0x20323172U, MiscKind::Rune, 12u, 0u, 1u, 1u }, // r12
	{ 0x20323173U, MiscKind::Rune, 12u, 1u, 1u, 1u }, // s12
	{ 0x20323272U, MiscKind::Rune, 22u, 0u, 1u, 1u }, // r22
	{ 0x20323273U, MiscKind::Rune, 22u, 1u, 1u, 1u }, // s22
	{ 0x20323372U, MiscKind::Rune, 32u, 0u, 1u, 1u }, // r32
	{ 0x20323373U, MiscKind::Rune, 32u, 1u, 1u, 1u }, // s32
	{ 0x20326175U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // ua2
	{ 0x20326178U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // xa2
	{ 0x20326B70U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // pk2
	{ 0x20326D75U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // um2
	{ 0x20327068U, MiscKind::Potion, 1u, 2u, 1u, 1u }, // hp2
	{ 0x2032706DU, MiscKind::Potion, 2u, 2u, 1u, 1u }, // mp2
	{ 0x20327274U, MiscKind::Quest, 0u, 0u, 2u, 2u }, // tr2
	{ 0x20333072U, MiscKind::Rune, 3u, 0u, 1u, 1u }, // r03
	{ 0x20333073U, MiscKind::Rune, 3u, 1u, 1u, 1u }, // s03
	{ 0x20333172U, MiscKind::Rune, 13u, 0u, 1u, 1u }, // r13
	{ 0x20333173U, MiscKind::Rune, 13u, 1u, 1u, 1u }, // s13
	{ 0x20333272U, MiscKind::Rune, 23u, 0u, 1u, 1u }, // r23
	{ 0x20333273U, MiscKind::Rune, 23u, 1u, 1u, 1u }, // s23
	{ 0x20333372U, MiscKind::Rune, 33u, 0u, 1u, 1u }, // r33
	{ 0x20333373U, MiscKind::Rune, 33u, 1u, 1u, 1u }, // s33
	{ 0x20336175U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // ua3
	{ 0x20336178U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // xa3
	{ 0x20336B70U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // pk3
	{ 0x20336D75U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // um3
	{ 0x20337068U, MiscKind::Potion, 1u, 3u, 1u, 1u }, // hp3
	{ 0x2033706DU, MiscKind::Potion, 2u, 3u, 1u, 1u }, // mp3
	{ 0x20343072U, MiscKind::Rune, 4u, 0u, 1u, 1u }, // r04
	{ 0x20343073U, MiscKind::Rune, 4u, 1u, 1u, 1u }, // s04
	{ 0x20343172U, MiscKind::Rune, 14u, 0u, 1u, 1u }, // r14
	{ 0x20343173U, MiscKind::Rune, 14u, 1u, 1u, 1u }, // s14
	{ 0x20343272U, MiscKind::Rune, 24u, 0u, 1u, 1u }, // r24
	{ 0x20343273U, MiscKind::Rune, 24u, 1u, 1u, 1u }, // s24
	{ 0x20343367U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // g34
	{ 0x2034336AU, MiscKind::Quest, 0u, 0u, 1u, 2u }, // j34
	{ 0x20346175U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // ua4
	{ 0x20346178U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // xa4
	{ 0x20346D75U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // um4
	{ 0x20347068U, MiscKind::Potion, 1u, 4u, 1u, 1u }, // hp4
	{ 0x2034706DU, MiscKind::Potion, 2u, 4u, 1u, 1u }, // mp4
	{ 0x20353072U, MiscKind::Rune, 5u, 0u, 1u, 1u }, // r05
	{ 0x20353073U, MiscKind::Rune, 5u, 1u, 1u, 1u }, // s05
	{ 0x20353172U, MiscKind::Rune, 15u, 0u, 1u, 1u }, // r15
	{ 0x20353173U, MiscKind::Rune, 15u, 1u, 1u, 1u }, // s15
	{ 0x20353272U, MiscKind::Rune, 25u, 0u, 1u, 1u }, // r25
	{ 0x20353273U, MiscKind::Rune, 25u, 1u, 1u, 1u }, // s25
	{ 0x20356175U, MiscKind::Quest, 0u, 0u, 1u, 2u }, // ua5
	{ 0x20356178U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // xa5
	{ 0x20356D75U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // um5
	{ 0x20357068U, MiscKind::Potion, 1u, 5u, 1u, 1u }, // hp5
	{ 0x2035706DU, MiscKind::Potion, 2u, 5u, 1u, 1u }, // mp5
	{ 0x20363072U, MiscKind::Rune, 6u, 0u, 1u, 1u }, // r06
	{ 0x20363073U, MiscKind::Rune, 6u, 1u, 1u, 1u }, // s06
	{ 0x20363172U, MiscKind::Rune, 16u, 0u, 1u, 1u }, // r16
	{ 0x20363173U, MiscKind::Rune, 16u, 1u, 1u, 1u }, // s16
	{ 0x20363272U, MiscKind::Rune, 26u, 0u, 1u, 1u }, // r26
	{ 0x20363273U, MiscKind::Rune, 26u, 1u, 1u, 1u }, // s26
	{ 0x20366D75U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // um6
	{ 0x20373072U, MiscKind::Rune, 7u, 0u, 1u, 1u }, // r07
	{ 0x20373073U, MiscKind::Rune, 7u, 1u, 1u, 1u }, // s07
	{ 0x20373172U, MiscKind::Rune, 17u, 0u, 1u, 1u }, // r17
	{ 0x20373173U, MiscKind::Rune, 17u, 1u, 1u, 1u }, // s17
	{ 0x20373272U, MiscKind::Rune, 27u, 0u, 1u, 1u }, // r27
	{ 0x20373273U, MiscKind::Rune, 27u, 1u, 1u, 1u }, // s27
	{ 0x20383072U, MiscKind::Rune, 8u, 0u, 1u, 1u }, // r08
	{ 0x20383073U, MiscKind::Rune, 8u, 1u, 1u, 1u }, // s08
	{ 0x20383172U, MiscKind::Rune, 18u, 0u, 1u, 1u }, // r18
	{ 0x20383173U, MiscKind::Rune, 18u, 1u, 1u, 1u }, // s18
	{ 0x20383272U, MiscKind::Rune, 28u, 0u, 1u, 1u }, // r28
	{ 0x20383273U, MiscKind::Rune, 28u, 1u, 1u, 1u }, // s28
	{ 0x20393072U, MiscKind::Rune, 9u, 0u, 1u, 1u }, // r09
	{ 0x20393073U, MiscKind::Rune, 9u, 1u, 1u, 1u }, // s09
	{ 0x20393172U, MiscKind::Rune, 19u, 0u, 1u, 1u }, // r19
	{ 0x20393173U, MiscKind::Rune, 19u, 1u, 1u, 1u }, // s19
	{ 0x20393272U, MiscKind::Rune, 29u, 0u, 1u, 1u }, // r29
	{ 0x20393273U, MiscKind::Rune, 29u, 1u, 1u, 1u }, // s29
	{ 0x20617331U, MiscKind::Gem, 3u, 7u, 1u, 1u }, // 1sa
	{ 0x20626262U, MiscKind::Quest, 0u, 0u, 2u, 2u }, // bbb
	{ 0x20626367U, MiscKind::Gem, 3u, 1u, 1u, 1u }, // gcb
	{ 0x20626667U, MiscKind::Gem, 3u, 2u, 1u, 1u }, // gfb
	{ 0x20626C67U, MiscKind::Gem, 3u, 4u, 1u, 1u }, // glb
	{ 0x20627067U, MiscKind::Gem, 3u, 5u, 1u, 1u }, // gpb
	{ 0x20627231U, MiscKind::Gem, 5u, 7u, 1u, 1u }, // 1rb
	{ 0x20627367U, MiscKind::Gem, 3u, 3u, 1u, 1u }, // gsb
	{ 0x20636B73U, MiscKind::Gem, 7u, 1u, 1u, 1u }, // skc
	{ 0x20637330U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // 0sc
	{ 0x20637369U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // isc
	{ 0x20637374U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // tsc
	{ 0x20646566U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // fed
	{ 0x20646B62U, MiscKind::Quest, 0u, 0u, 2u, 2u }, // bkd
	{ 0x20646D67U, MiscKind::Gem, 6u, 6u, 1u, 1u }, // gmd
	{ 0x20647473U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // std
	{ 0x20647474U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // ttd
	{ 0x20656369U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // ice
	{ 0x20656D67U, MiscKind::Gem, 4u, 6u, 1u, 1u }, // gme
	{ 0x20657073U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // spe
	{ 0x20666B73U, MiscKind::Gem, 7u, 2u, 1u, 1u }, // skf
	{ 0x20667068U, MiscKind::Potion, 1u, 6u, 1u, 1u }, // hpf
	{ 0x2066706DU, MiscKind::Potion, 2u, 6u, 1u, 1u }, // mpf
	{ 0x20676162U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // bag
	{ 0x20676367U, MiscKind::Gem, 4u, 1u, 1u, 1u }, // gcg
	{ 0x20676667U, MiscKind::Gem, 4u, 2u, 1u, 1u }, // gfg
	{ 0x20676B64U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // dkg
	{ 0x20676B68U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // hkg
	{ 0x20676B74U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // tkg
	{ 0x20676C66U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // flg
	{ 0x20676C67U, MiscKind::Gem, 4u, 4u, 1u, 1u }, // glg
	{ 0x20676E66U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // fng
	{ 0x20677067U, MiscKind::Gem, 4u, 5u, 1u, 1u }, // gpg
	{ 0x20677367U, MiscKind::Gem, 4u, 3u, 1u, 1u }, // gsg
	{ 0x2068636BU, MiscKind::Tool, 0u, 0u, 1u, 1u }, // kch
	{ 0x20686563U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // ceh
	{ 0x20696431U, MiscKind::Gem, 6u, 7u, 1u, 1u }, // 1di
	{ 0x206B6269U, MiscKind::Tool, 0u, 0u, 1u, 2u }, // ibk
	{ 0x206B6274U, MiscKind::Tool, 0u, 0u, 1u, 2u }, // tbk
	{ 0x206B6D67U, MiscKind::Gem, 7u, 6u, 1u, 1u }, // gmk
	{ 0x206B7331U, MiscKind::Gem, 7u, 7u, 1u, 1u }, // 1sk
	{ 0x206C6174U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // tal
	{ 0x206C6B73U, MiscKind::Gem, 7u, 4u, 1u, 1u }, // skl
	{ 0x206C6C71U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // qll
	{ 0x206C6F73U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // sol
	{ 0x206C7062U, MiscKind::Potion, 2u, 2u, 1u, 1u }, // bpl
	{ 0x206C7072U, MiscKind::Potion, 1u, 2u, 1u, 1u }, // rpl
	{ 0x206C7672U, MiscKind::Potion, 3u, 2u, 1u, 1u }, // rvl
	{ 0x206D6131U, MiscKind::Gem, 1u, 7u, 1u, 1u }, // 1am
	{ 0x206D6531U, MiscKind::Gem, 4u, 7u, 1u, 1u }, // 1em
	{ 0x206D6D67U, MiscKind::Gem, 1u, 6u, 1u, 1u }, // gmm
	{ 0x206E6864U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // dhn
	{ 0x206E7268U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // hrn
	{ 0x206F6D67U, MiscKind::Gem, 8u, 6u, 1u, 1u }, // gmo
	{ 0x206F7068U, MiscKind::Potion, 1u, 3u, 1u, 1u }, // hpo
	{ 0x206F706DU, MiscKind::Potion, 2u, 3u, 1u, 1u }, // mpo
	{ 0x206F7331U, MiscKind::Gem, 8u, 7u, 1u, 1u }, // 1so
	{ 0x2070666DU, MiscKind::Potion, 6u, 0u, 1u, 1u }, // mfp
	{ 0x20706870U, MiscKind::Potion, 6u, 0u, 1u, 1u }, // php
	{ 0x20707078U, MiscKind::Potion, 6u, 0u, 1u, 1u }, // xpp
	{ 0x20707363U, MiscKind::Potion, 6u, 0u, 1u, 1u }, // csp
	{ 0x20707366U, MiscKind::Potion, 6u, 0u, 1u, 1u }, // fsp
	{ 0x2070736CU, MiscKind::Potion, 6u, 0u, 1u, 1u }, // lsp
	{ 0x2070736DU, MiscKind::Potion, 6u, 0u, 1u, 1u }, // msp
	{ 0x20707370U, MiscKind::Potion, 6u, 0u, 1u, 1u }, // psp
	{ 0x20707431U, MiscKind::Gem, 2u, 7u, 1u, 1u }, // 1tp
	{ 0x20707572U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // rup
	{ 0x2070776AU, MiscKind::Tool, 0u, 0u, 1u, 1u }, // jwp
	{ 0x2072626DU, MiscKind::Quest, 0u, 0u, 1u, 1u }, // mbr
	{ 0x20726271U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // qbr
	{ 0x20726367U, MiscKind::Gem, 5u, 1u, 1u, 1u }, // gcr
	{ 0x20726667U, MiscKind::Gem, 5u, 2u, 1u, 1u }, // gfr
	{ 0x20726761U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // agr
	{ 0x20726765U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // egr
	{ 0x2072676BU, MiscKind::Tool, 0u, 0u, 1u, 1u }, // kgr
	{ 0x2072676DU, MiscKind::Tool, 0u, 0u, 1u, 1u }, // mgr
	{ 0x2072676FU, MiscKind::Tool, 0u, 0u, 1u, 1u }, // ogr
	{ 0x20726772U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // rgr
	{ 0x20726773U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // sgr
	{ 0x20726774U, MiscKind::Tool, 0u, 0u, 1u, 1u }, // tgr
	{ 0x20726871U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // qhr
	{ 0x20726C67U, MiscKind::Gem, 5u, 4u, 1u, 1u }, // glr
	{ 0x20726D67U, MiscKind::Gem, 5u, 6u, 1u, 1u }, // gmr
	{ 0x20727067U, MiscKind::Gem, 5u, 5u, 1u, 1u }, // gpr
	{ 0x20727367U, MiscKind::Gem, 5u, 3u, 1u, 1u }, // gsr
	{ 0x20736574U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // tes
	{ 0x20736B62U, MiscKind::Quest, 0u, 0u, 2u, 2u }, // bks
	{ 0x20736D67U, MiscKind::Gem, 3u, 6u, 1u, 1u }, // gms
	{ 0x20736D77U, MiscKind::Potion, 6u, 1u, 1u, 1u }, // wms
	{ 0x20737062U, MiscKind::Potion, 2u, 1u, 1u, 1u }, // bps
	{ 0x20737072U, MiscKind::Potion, 1u, 1u, 1u, 1u }, // rps
	{ 0x20737076U, MiscKind::Potion, 4u, 1u, 1u, 1u }, // vps
	{ 0x20737079U, MiscKind::Potion, 5u, 1u, 1u, 1u }, // yps
	{ 0x20737361U, MiscKind::Quest, 0u, 0u, 2u, 2u }, // ass
	{ 0x2073736DU, MiscKind::Quest, 0u, 0u, 1u, 1u }, // mss
	{ 0x20737672U, MiscKind::Potion, 3u, 1u, 1u, 1u }, // rvs
	{ 0x20746562U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // bet
	{ 0x20746D67U, MiscKind::Gem, 2u, 6u, 1u, 1u }, // gmt
	{ 0x20747268U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // hrt
	{ 0x20747473U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // stt
	{ 0x20756B73U, MiscKind::Gem, 7u, 3u, 1u, 1u }, // sku
	{ 0x20766367U, MiscKind::Gem, 1u, 1u, 1u, 1u }, // gcv
	{ 0x20766667U, MiscKind::Gem, 1u, 2u, 1u, 1u }, // gfv
	{ 0x20767067U, MiscKind::Gem, 1u, 5u, 1u, 1u }, // gpv
	{ 0x20767367U, MiscKind::Gem, 1u, 3u, 1u, 1u }, // gsv
	{ 0x2076756CU, MiscKind::Tool, 0u, 0u, 1u, 2u }, // luv
	{ 0x20767A67U, MiscKind::Gem, 1u, 4u, 1u, 1u }, // gzv
	{ 0x2077616AU, MiscKind::Quest, 0u, 0u, 1u, 1u }, // jaw
	{ 0x20776367U, MiscKind::Gem, 6u, 1u, 1u, 1u }, // gcw
	{ 0x20776667U, MiscKind::Gem, 6u, 2u, 1u, 1u }, // gfw
	{ 0x20776C67U, MiscKind::Gem, 6u, 4u, 1u, 1u }, // glw
	{ 0x20777067U, MiscKind::Gem, 6u, 5u, 1u, 1u }, // gpw
	{ 0x20777367U, MiscKind::Gem, 6u, 3u, 1u, 1u }, // gsw
	{ 0x20786F62U, MiscKind::Tool, 0u, 0u, 2u, 2u }, // box
	{ 0x20796367U, MiscKind::Gem, 2u, 1u, 1u, 1u }, // gcy
	{ 0x20796562U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // bey
	{ 0x2079656BU, MiscKind::Tool, 0u, 0u, 1u, 1u }, // key
	{ 0x20796571U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // qey
	{ 0x20796667U, MiscKind::Gem, 2u, 2u, 1u, 1u }, // gfy
	{ 0x20796C67U, MiscKind::Gem, 2u, 4u, 1u, 1u }, // gly
	{ 0x20797067U, MiscKind::Gem, 2u, 5u, 1u, 1u }, // gpy
	{ 0x20797367U, MiscKind::Gem, 2u, 3u, 1u, 1u }, // gsy
	{ 0x207A6373U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // scz
	{ 0x207A6B73U, MiscKind::Gem, 7u, 5u, 1u, 1u }, // skz
	{ 0x207A7262U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // brz
	{ 0x207A7965U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // eyz
	{ 0x207A7978U, MiscKind::Quest, 0u, 0u, 1u, 1u }, // xyz
};

} // namespace Quyen194ItemSorting::Classification
