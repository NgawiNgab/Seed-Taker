#pragma once

// SeedTaker's feature module. Capture status: the PACKET path is live
// (slot-37 hook -> StartGamePacket+0xA8, INFERRED until runtime-validated)
// and the SRC-SCAN path is live (getter-hook -> 7 int64 windows over the
// src settings cluster, F-071/F-072); the OBJECT path (LevelData.+0x3b0
// via Level::initialize) stays prepared-but-disabled as backup (vehicle
// retired, F-067). See capture/SeedCapture.hpp, capture/SrcScan.hpp,
// docs/IMPLEMENTATION_PLAN.md, and docs/EVIDENCE_LOG.md (F-048..F-072).
//
// SPEC §4 is enforced in onFrame: no number is rendered while unavailable,
// and inferred values always carry an explicit INFERRED marking. Packet and
// object readings are tracked separately and a divergence is shown, never
// silently merged (SPEC spoof separation).

#include "../Module.hpp"
#include "modules/ModuleRegistry.hpp"
#include <seedtaker/sdk/offsets/Seed.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace seedtaker {

enum class SeedConfidence {
    Unavailable, // No value; a number must never be displayed.
    Inferred,    // Derived heuristically; must be marked INFERRED.
    Recovered,   // Read directly from a verified source.
};

struct SeedReading {
    // Bedrock seeds are signed 32-bit on the wire (Start Game Seed is a
    // SignedVarInt); int64 preserves negative seeds exactly.
    std::optional<std::int64_t> value;
    std::string source = "none";
    SeedConfidence confidence = SeedConfidence::Unavailable;
    std::string detail = "capture: idle";
};

class SeedModule : public Module {
public:
    SeedModule();
    void onInit() override;
    void onEnable() override;
    void onDisable() override;
    void onFrame() override;
    bool onMenuConfigChanged(std::string_view key, std::string_view value) override;
    void loadConfig(const nlohmann::json& j) override;
    void saveConfig(nlohmann::json& j) override;

    const SeedReading& reading() const { return m_reading; }
    std::optional<std::int64_t> seed() const { return m_reading.value; }
    const std::string& status() const { return m_reading.detail; }

private:
    struct SourceState {
        std::uint64_t sequence = 0;
        std::int32_t value = 0;
        bool verified = false; // Runtime-validated against a known seed.
        bool failed = false;   // Failed validation; excluded until re-armed.
        // Snapshot of the one-shot comparison for HUD display (logcat is
        // not reachable for most on-device users, so "see log" alone is
        // a dead end — the values must be visible here).
        std::int64_t mismatchExpected = 0;
        std::int32_t mismatchGot = 0;
    };

    void drainCapture();
    void renderReading(std::vector<DrawCommand>& commands, float& y) const;

    SeedReading m_reading;
    SourceState m_packet;
    SourceState m_object;
    // Src-scan windows (F-071/F-072): one state per int64 snapshot window.
    // Each window one-shots independently against the expected seed; a
    // full-int64 match is runtime validation (Recovered).
    struct WindowState {
        std::uint64_t sequence = 0;
        std::int64_t value = 0;
        bool verified = false;
        bool failed = false;
        bool latched = false; // One-shot comparison done for this arming.
    };
    std::array<WindowState, sdk::offsets::seed::SrcScanWindowCount> m_srcWindows;
    // One-shot validation: the first reading per source after arming is
    // compared against m_expectedSeed. Match -> verified (Recovered);
    // mismatch -> source failed (Unavailable, sticky until re-armed).
    bool m_validationArmed = false;
    bool m_validationPassed = false;
    std::int64_t m_expectedSeed = 0;
    bool m_hasExpectedSeed = false;
    float m_hudPosX = 20.0f;
    float m_hudPosY = 60.0f;
    bool m_isHudModule = true;
    bool m_showHud = true;
    float m_textSize = 32.0f;
};

}
