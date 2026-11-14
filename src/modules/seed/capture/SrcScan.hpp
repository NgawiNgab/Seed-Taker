#pragma once

// Src-scan: 64-bit seed window scanner (design record F-067/F-071/F-072).
//
// Why this exists: the world seed is 64-bit (F-058) and lives at src+0x0
// (F-087: ab28 getter, "WorldSeed"-labeled) — no direct LevelData member
// holds it (F-086: container-exclusive home). The hook sits on the
// LevelData ctor (file VA + slide, dual `bl`-word anchors verified): a
// preserve-all ASM wrapper snapshots 8 int64 windows at entry — src(x1)+0x0
// (the seed prediction) plus 0x60..0x78 as negative controls — then
// tail-calls the original with bit-identical behavior (F-074).
// (First attempt hooked the 8-byte src getter and crashed the game —
// patch overrun into the neighbor function. F-073. Never again.)
//
// Mapping safety: src is the live input object; +0x0 is its own base
// (the ctor derefs it via ab28 itself — mapped by construction) and every
// other window sits in a 16B chunk anchored by a known game read (F-071).
//
// Threading: per-window value+sequence atomics (lock-free). The detour
// allocates/locks/logs nothing. SeedModule drains by sequence and
// one-shot-validates each window against the expected seed.

#include <cstddef>
#include <cstdint>

namespace seedtaker::capture {

struct SrcWindowReading {
    std::uint64_t sequence = 0; // 0 = no snapshot yet.
    std::int64_t value = 0;
};

class SrcScan {
public:
    // Idempotent. Returns true when the getter hook is active.
    // Never fails hard: failures stay inactive and are reported
    // through status().
    static bool install();
    static void uninstall();

    static bool active();
    static SrcWindowReading windowReading(std::size_t window);
    static const char* status(); // Short static string; safe to display.

    SrcScan() = delete;
};

}
