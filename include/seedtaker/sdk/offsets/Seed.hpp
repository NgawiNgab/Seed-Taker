#pragma once

// Evidence-backed seed-capture constants for MCPE 1.26.62 (arm64).
//
// Every value below was traced in the 1.26.62 binary via Zaphkiel (see
// docs/EVIDENCE_LOG.md; finding IDs cited per constant). Nothing here is
// copied from BedrockTools or any other build. Seed-ness of the captured
// int32 is INFERRED-strong (F-051), NOT confirmed: consumers must report it
// through the confidence pipeline (SeedReading) and never as a bare fact.

#include <cstddef>

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

}
