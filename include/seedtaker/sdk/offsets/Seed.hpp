#pragma once

// Evidence-backed seed-capture constants for MCPE 1.26.62 (arm64).
//
// Every value below was traced in the 1.26.62 binary via Zaphkiel (see
// docs/EVIDENCE_LOG.md; finding IDs cited per constant). Nothing here is
// copied from BedrockTools or any other build. Seed-ness of the captured
// int32 is INFERRED-strong (F-051), NOT confirmed: consumers must report it
// through the confidence pipeline (SeedReading) and never as a bare fact.

#include <cstddef>
#include <cstdint>

namespace seedtaker::sdk::offsets::seed {

// Flat Itanium RTTI name of the level-data holder (F-010, ST-001).
inline constexpr char LevelDataRtti[] = "9LevelData";
// Flat RTTI name of the client packet handler (F-033, ST-005).
inline constexpr char HandlerRtti[] = "26LegacyClientNetworkHandler";

// LevelData vtable slot returning the int32 at +0x3b0 (F-024, ST-003).
inline constexpr std::size_t LevelDataSeedSlot = 8;
// Adjacent set-once slot; used only to dual-verify the vtable base (F-024).
inline constexpr std::size_t LevelDataSeedSetterSlot = 9;
// The int32 field itself, relative to the LevelData instance (F-024).
inline constexpr std::size_t LevelDataSeedOffset = 0x3b0;

// LegacyClientNetworkHandler vtable slot for handle(id, StartGamePacket),
// reached from the dispatcher via [handler_vtable+0x128] (F-028/F-033).
inline constexpr std::size_t HandlerStartGameSlot = 37;
// StartGame packet id, returned by slot 2 (F-027, ST-003). The capture
// detour requires this id before trusting the seed offset (F-055).
inline constexpr int StartGamePacketId = 0x0B;
inline constexpr std::size_t StartGameIdSlot = 2;
// Packet-relative int32 traced packet -> src+0x78 -> LevelData.+0x3b0
// (payload+0x78, payload base = packet+0x30; F-049, ST-008).
inline constexpr std::size_t StartGameSeedOffset = 0xA8;

// LevelSettings int32 getter, file VA (F-048): `ldr w0,[x0,#0x78];ret`.
// NEVER HOOK THIS (F-073): the 8-byte function was hooked first and the
// ≥12-byte inline patch destroyed the neighbor 0x10b1da10 → SIGILL.
// Kept as the anchor-target reference (see the ctor `bl` words below).
inline constexpr std::uintptr_t SrcGetterFileVA = 0x10b1da08;
// LevelData ctor, file VA (F-045): the SrcScan hook target (F-074).
// ~4KB with a straight-line 16B prologue — ample patch room,
// trampoline-safe. Live = libraryBase("libminecraftpe.so") + VA (ET_DYN).
inline constexpr std::uintptr_t LevelDataCtorFileVA = 0x11146b24;
// Pre-hook anchors: two `bl` words inside the ctor (ST-007), verified to
// prove slide + function identity (fail-safe: refuse on mismatch).
// 0x11146df4: `bl 0x10b1da08` → 0x97E75B05; 0x11146da8: `bl 0x10b1db08`
// → 0x97E75B58 (machine-verified; hand computation errs — cross-check).
inline constexpr std::uintptr_t LevelDataCtorBlSite1 = 0x11146df4;
inline constexpr std::uint32_t LevelDataCtorBlWord1 = 0x97E75B05u;
inline constexpr std::uintptr_t LevelDataCtorBlSite2 = 0x11146da8;
inline constexpr std::uint32_t LevelDataCtorBlWord2 = 0x97E75B58u;
// 7 int64 snapshot windows over the src cluster (F-071). Every window
// lies in a 16B chunk anchored by a known game read (0x64 / 0x78),
// so all reads stay mapped by malloc granularity (16B, Android arm64).
inline constexpr std::size_t SrcScanWindowCount = 7;
inline constexpr std::size_t SrcScanWindows[SrcScanWindowCount] = {0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78};

}
