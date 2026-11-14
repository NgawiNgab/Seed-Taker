#pragma once

// Src-scan: 64-bit seed window scanner (design record F-067/F-071/F-072).
//
// Why this exists: the world seed is 64-bit (F-058) but no fully-mirrored
// 64-bit window exists in LevelData (F-071) — so the seed must be read
// from the src settings object directly. The hook sits on the src getter
// 0x10b1da08 (file VA + slide, body-verified): known 1-arg signature, so
// the detour is plain C++ with zero register-shape risk (the ctor-ASM
// trampoline was considered and discarded, F-072). Every call proves
// src+0x78 mapped, so the post-call snapshot of 7 int64 windows over
// src+0x60..0x78 is always safe (16B-chunk argument, F-071).
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
