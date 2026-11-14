#pragma once

// Evidence-backed seed-capture constants for MCPE 1.26.62 (arm64).
//
// Every value below was traced in the 1.26.62 binary via Zaphkiel (see
// docs/EVIDENCE_LOG.md; finding IDs cited per constant). Nothing here is
// copied from BedrockTools or any other build. The int32 slot/offset
// consts below are the LEGACY 32-bit seed slot (F-051 REFUTED by F-085 —
// real but seedless); the live 64-bit seed is src+0x0 (F-087). Consumers
// must report through the confidence pipeline (SeedReading), never bare.

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
// Src 64-bit seed getter, file VA (F-087): `ldr x0,[x0];ret` — the seed
// is src+0x0 (a caller files it under the literal key "WorldSeed").
// NEVER HOOK THIS (F-073 rule): 8 bytes, same killer class as da08.
// Reference only — the seed is read via the ctor-hook window, not a call.
inline constexpr std::uintptr_t SrcSeedGetterFileVA = 0x10b0ab28;
// LevelData seed setter, file VA (F-086): registers the u64 seed as a
// type-7 gamerule in this+0x4f8 (14 callers: NBT load, ctor, +12). This is
// the single registration point for the seed — every load path (SP disk,
// LAN, Survival Server packet) funnels through it, so hooking it captures
// the authoritative 64-bit seed path-agnostically (F-088). Standard
// prologue (~0x110 bytes) — safely hookable (unlike the 8-byte getters).
// Called as setter(this, seed); the seed arrives in x1 (AAPCS64).
inline constexpr std::uintptr_t LevelDataSeedSetterFileVA = 0x111463bc;
// Pre-hook anchors for the setter (ST-012 P0): two `bl` words inside the
// setter, script-computed from the disassembly (the bl-encoding formula
// was verified by reproducing the known ctor anchor 0x97E75B05).
// 0x11146414: `bl 0xd05546c` (registrar) -> 0x96FC3C16;
// 0x11146494: `bl 0x1114e3e0` (notify) -> 0x94001FD3.
inline constexpr std::uintptr_t LevelDataSeedSetterBlSite1 = 0x11146414;
inline constexpr std::uint32_t LevelDataSeedSetterBlWord1 = 0x96FC3C16u;
inline constexpr std::uintptr_t LevelDataSeedSetterBlSite2 = 0x11146494;
inline constexpr std::uint32_t LevelDataSeedSetterBlWord2 = 0x94001FD3u;
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
// 8 int64 snapshot windows (F-071 + F-087): window 0 is src+0x0 — the u64
// seed itself (ab28 getter, "WorldSeed"-labeled); windows 1-7 re-scan the
// 0x60..0x78 cluster as negative controls (0/7 in FIELD-001). +0x0 is the
// object base of the live src arg (the ctor derefs it via ab28 itself),
// so it is mapped by construction; the rest sit in 16B chunks anchored
// by known game reads (F-071). A +0x0 match with 0/7 controls = perfect
// discrimination (promotes to Recovered through existing validation).
inline constexpr std::size_t SrcScanWindowCount = 8;
inline constexpr std::size_t SrcScanWindows[SrcScanWindowCount] = {0x0, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78};

}
