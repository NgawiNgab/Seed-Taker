#pragma once

// Src-scan: 64-bit seed window scanner (design record F-067/F-071/F-072).
//
// Why this exists: the world seed is 64-bit (F-058) but no fully-mirrored
// 64-bit window exists in LevelData (F-071) — so the seed must be read
// from the src settings object directly. The hook sits on the LevelData
// ctor (file VA + slide, dual `bl`-word anchors verified): a preserve-all
// ASM wrapper snapshots 7 int64 windows over src(x1)+0x60..0x78 at entry,
// then tail-calls the original with bit-identical behavior (F-074).
// (First attempt hooked the 8-byte src getter and crashed the game —
// patch overrun into the neighbor function. F-073. Never again.)
//
// Mapping safety: src is the live input object (the ctor reads src+0x78
// itself); every window additionally sits in a 16B chunk anchored by a
// known game read (F-071).
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
