#pragma once

// Seed capture coordinator: installs the native hooks, validates what they
// observe, and publishes readings to the HUD thread. Design record: F-052,
// F-055, docs/IMPLEMENTATION_PLAN.md.
//
// Sources (kept separate for SPEC spoof separation):
//   packet — StartGamePacket+0xA8 read in the slot-37 detour, gated on
//            getId() == 0x0B (F-027/F-055). INFERRED until validated.
//   object — LevelData.+0x3b0 read via slot 8 on the levelSettings object
//            captured by the Level::initialize detour, gated on the
//            vtable-base check (F-055). PENDING ST-009 (no LevelInit
//            address yet); the path reports disabled until then.
//
// Threading: detours run on game threads and publish through a single
// packed atomic per source (sequence high 32, seed low 32) — lock-free and
// tear-free. SeedModule (frame thread) drains by sequence number. The
// detours allocate nothing, lock nothing, and log nothing.

#include <cstdint>

namespace seedtaker::capture {

struct CaptureReading {
    std::uint64_t sequence = 0; // 0 = no reading yet.
    std::int32_t seed = 0;
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
    static CaptureReading packetReading();
    static CaptureReading objectReading();
    // Diagnostic counters (monotonic; for HUD detail + validation).
    static std::uint64_t rejectedPackets();      // slot-37 calls failing the getId gate.
    static std::uint64_t typeMismatches();       // levelSettings failing the vtable check.
    static const char* status();                 // Short static string; safe to display.

    SeedCapture() = delete;
};

}
