#pragma once

// Seed capture coordinator: installs the native hooks, validates what they
// observe, and publishes readings to the HUD thread. Design record: F-052,
// F-055, docs/IMPLEMENTATION_PLAN.md.
//
// Sources (kept separate for SPEC spoof separation):
//   setter — LevelData seed setter detour: the u64 seed arrives in x1 and
//            is published whole (F-086/F-088). This is PRIMARY: the setter
//            is the single registration point for the seed, so it is the
//            authoritative value on every load path (SP disk, LAN,
//            Survival Server packet). Full int64 — no truncation.
//   packet — StartGamePacket+0xA8 read in the slot-37 detour, gated on
//            getId() == 0x0B (F-027/F-055). Legacy int32 cross-check only:
//            it read 0 on the big Survival Server (usr report), the field
//            is not the 64-bit seed.
//   object — LevelData.+0x3b0 read via slot 8 on the levelSettings object
//            captured by the Level::initialize detour, gated on the
//            vtable-base check (F-055). PENDING ST-009 (no LevelInit
//            address yet); the path reports disabled until then.
//
// Threading: detours run on game threads. The legacy sources publish one
// packed atomic each (sequence high 32, seed low 32); the setter publishes
// a full int64 through a seqlock, since the seed does not fit in 32 bits.
// SeedModule (frame thread) drains by sequence number. The detours allocate
// nothing, lock nothing, and log nothing.

#include <cstdint>

namespace seedtaker::capture {

struct CaptureReading {
    std::uint64_t sequence = 0; // 0 = no reading yet.
    std::int32_t seed = 0;
};

// Full int64 reading (setter source, F-088): unlike CaptureReading the
// seed is not truncated to 32 bits, so out-of-int32-range seeds are
// representable. sequence/value are published through a seqlock
// (odd = write in progress), so a read that overlaps a write is retried
// rather than torn.
struct SetterReading {
    std::uint64_t sequence = 0; // 1,3,5... = write in flight; even = settled.
    std::int64_t seed = 0;
};

class SeedCapture {
public:
    // Idempotent. Returns true when at least one hook is active.
    // Never fails hard: unresolvable targets stay disabled and are
    // reported through status().
    static bool install();
    static void uninstall();

    static bool packetHookActive();
    static bool levelInitHookActive();
    static bool setterHookActive();
    static CaptureReading packetReading();
    static CaptureReading objectReading();
    static SetterReading setterReading();
    // Diagnostic counters (monotonic; for HUD detail + validation).
    static std::uint64_t rejectedPackets();      // slot-37 calls failing the getId gate.
    static std::uint64_t typeMismatches();       // levelSettings failing the vtable check.
    static const char* status();                 // Short static string; safe to display.

    SeedCapture() = delete;
};

}
