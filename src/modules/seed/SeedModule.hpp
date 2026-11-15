#pragma once

// SeedTaker's feature module. Capture status: the SETTER path is primary
// (seed-setter detour -> authoritative u64 seed, F-088) and the SRC-SCAN
// path is live (ctor hook -> 8 int64 windows over src, F-071/F-072/F-087);
// the PACKET path (StartGamePacket+0xA8, legacy int32) and OBJECT path
// (LevelData.+0x3b0) are retained as legacy cross-checks only. See
// capture/SeedCapture.hpp, capture/SrcScan.hpp, docs/IMPLEMENTATION_PLAN.md,
// and docs/EVIDENCE_LOG.md (F-048..F-088).
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
    // Setter source (F-088): the authoritative u64 seed captured from the
    // LevelData seed-setter detour. Revalidated on every new registration
    // (last call wins), unlike the one-shot src windows.
    struct SetterState {
        std::uint64_t sequence = 0;
        std::int64_t value = 0;
        bool verified = false;
        bool failed = false;
        // Recorded on mismatch (F-089): without these the HUD fell through
        // to the legacy int32 legs and printed a placeholder instead of the
        // real "setter registered X, expected Y" contradiction.
        std::int64_t mismatchExpected = 0;
        std::int64_t mismatchGot = 0;
    };
    // F-089 follow-up: an expectation of 0 is not a real expectation. The
    // src+0x64 window is all-zero on every path, so "armed, expected 0"
    // validated a permanently-zero window and reported 0 as RECOVERED. A 0
    // expectation is a no-op, never a match.
    bool m_expectedSeedUsable() const { return m_hasExpectedSeed && m_expectedSeed != 0; }
    SetterState m_setter;
    // Packet band (F-092): per-slot state for the decoded StartGame payload
    // candidates at LevelSettings+0x0C/+0x14. One-shot validation like the
    // src windows, but this is the ONLY band a server fills.
    struct BandState {
        std::uint64_t sequence = 0;
        std::size_t offset = 0;
        std::int64_t value = 0;
        bool verified = false;
        bool failed = false;
    };
    BandState m_band[sdk::offsets::seed::PacketSeedBandCount];
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
