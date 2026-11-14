#include "SeedModule.hpp"
#include "capture/SeedCapture.hpp"
#include "modules/ModuleRegistry.hpp"
#include <pl/Logger.hpp>
#include <cstdlib>

namespace seedtaker {

SeedModule::SeedModule()
    : Module("Seed Taker", "Captures and displays the world seed.") {
    // Default-on: the module is a passive observer (capture + forward), and
    // the capture reports unavailable until hooks resolve. The user can
    // toggle it off; config may override after load.
    setMasterEnabled(true);
}

void SeedModule::onInit() {
    // Hooks install from onEnable (BedrockTools module-owned-hooks pattern);
    // onInit stays empty so re-registration never double-installs.
}

void SeedModule::onEnable() {
    capture::SeedCapture::install();
}

void SeedModule::onDisable() {
    capture::SeedCapture::uninstall();
}

void SeedModule::drainCapture() {
    const auto packet = capture::SeedCapture::packetReading();
    if (packet.sequence != 0 && packet.sequence != m_packet.sequence) {
        m_packet.sequence = packet.sequence;
        m_packet.value = packet.seed;
        if (m_validationArmed && m_hasExpectedSeed && !m_packet.verified && !m_packet.failed) {
            if (static_cast<std::int64_t>(packet.seed) == m_expectedSeed) {
                m_packet.verified = true;
                m_validationPassed = true;
            } else {
                m_packet.failed = true;
                pl::log::Logger::getOrCreate("SeedTaker")
                    .warn("Seed validation MISMATCH (packet): expected "
                          + std::to_string(m_expectedSeed) + ", got " + std::to_string(packet.seed));
            }
        }
    }
    const auto object = capture::SeedCapture::objectReading();
    if (object.sequence != 0 && object.sequence != m_object.sequence) {
        m_object.sequence = object.sequence;
        m_object.value = object.seed;
        if (m_validationArmed && m_hasExpectedSeed && !m_object.verified && !m_object.failed) {
            if (static_cast<std::int64_t>(object.seed) == m_expectedSeed) {
                m_object.verified = true;
                m_validationPassed = true;
            } else {
                m_object.failed = true;
                pl::log::Logger::getOrCreate("SeedTaker")
                    .warn("Seed validation MISMATCH (object): expected {}, got {}", m_expectedSeed, object.seed);
            }
        }
    }

    // Primary = newest usable reading across sources. Failed sources are
    // excluded; divergences are surfaced at render time, never merged.
    const bool packetUsable = m_packet.sequence != 0 && !m_packet.failed;
    const bool objectUsable = m_object.sequence != 0 && !m_object.failed;
    m_reading.value.reset();
    m_reading.confidence = SeedConfidence::Unavailable;
    m_reading.source = "none";
    m_reading.detail = capture::SeedCapture::status();
    const SourceState* primary = nullptr;
    const char* primarySource = nullptr;
    if (packetUsable && (!objectUsable || m_packet.sequence >= m_object.sequence)) {
        primary = &m_packet;
        primarySource = "server-packet:StartGamePacket+0xA8";
    } else if (objectUsable) {
        primary = &m_object;
        primarySource = "level-object:LevelData+0x3b0";
    }
    if (primary) {
        m_reading.value = static_cast<std::int64_t>(primary->value);
        m_reading.source = primarySource;
        if (primary->verified) {
            m_reading.confidence = SeedConfidence::Recovered;
            m_reading.detail = "runtime-validated";
        } else {
            m_reading.confidence = SeedConfidence::Inferred;
            m_reading.detail = "unconfirmed field (F-051)";
        }
    } else if (m_packet.failed || m_object.failed) {
        m_reading.detail = "validation mismatch (see log)";
    }
}

void SeedModule::renderReading(std::vector<DrawCommand>& commands, float& y) const {
    // SPEC §4: an inferred value is never rendered without an explicit
    // INFERRED marking, and no number is shown while unavailable.
    std::string text;
    if (!m_reading.value || m_reading.confidence == SeedConfidence::Unavailable) {
        text = "Seed: unavailable (" + m_reading.detail + ")";
    } else if (m_reading.confidence == SeedConfidence::Recovered) {
        text = "Seed: " + std::to_string(*m_reading.value) + " [recovered: " + m_reading.source + "]";
    } else {
        text = "Seed: ~" + std::to_string(*m_reading.value)
            + " (INFERRED - " + m_reading.source + ": " + m_reading.detail + ")";
    }
    DrawCommand cmd{};
    cmd.type = DrawCommandType::Text;
    cmd.x = m_hudPosX;
    cmd.y = y;
    cmd.w = 900.0f;
    cmd.h = m_textSize + 8.0f;
    cmd.color = 0xFFFFFFFF;
    cmd.size = m_textSize;
    cmd.text = text;
    commands.push_back(cmd);
    y += m_textSize + 8.0f;

    // Spoof separation: packet and object values are shown side by side when
    // both exist and disagree.
    const bool packetUsable = m_packet.sequence != 0 && !m_packet.failed;
    const bool objectUsable = m_object.sequence != 0 && !m_object.failed;
    if (packetUsable && objectUsable && m_packet.value != m_object.value) {
        DrawCommand diverge{};
        diverge.type = DrawCommandType::Text;
        diverge.x = m_hudPosX;
        diverge.y = y;
        diverge.w = 900.0f;
        diverge.h = m_textSize + 8.0f;
        diverge.color = 0xFFFFAA00;
        diverge.size = m_textSize;
        diverge.text = "DIVERGE: packet=" + std::to_string(m_packet.value)
            + " object=" + std::to_string(m_object.value);
        commands.push_back(diverge);
        y += m_textSize + 8.0f;
    }

    if (m_validationArmed && !m_validationPassed) {
        DrawCommand validation{};
        validation.type = DrawCommandType::Text;
        validation.x = m_hudPosX;
        validation.y = y;
        validation.w = 900.0f;
        validation.h = m_textSize + 8.0f;
        validation.color = 0xFFAAAAFF;
        validation.size = m_textSize;
        validation.text = m_hasExpectedSeed
            ? "Validate: armed, expected " + std::to_string(m_expectedSeed)
            : "Validate: armed, no expected seed set";
        commands.push_back(validation);
        y += m_textSize + 8.0f;
    }
}

void SeedModule::onFrame() {
    if (!enabled || !m_showHud) return;
    drainCapture();
    std::vector<DrawCommand> commands;
    float y = m_hudPosY;
    renderReading(commands, y);
    submitDrawCommands(moduleId, commands);
}

void SeedModule::loadConfig(const nlohmann::json& j) {
    Module::loadConfig(j);
    if (j.contains("hudPosX")) m_hudPosX = j["hudPosX"].get<float>();
    if (j.contains("hudPosY")) m_hudPosY = j["hudPosY"].get<float>();
    if (j.contains("isHudModule")) m_isHudModule = j["isHudModule"].get<bool>();
    if (j.contains("m_showHud")) m_showHud = j["m_showHud"].get<bool>();
    if (j.contains("m_textSize")) m_textSize = j["m_textSize"].get<float>();
    if (j.contains("validationArmed")) {
        const bool armed = j["validationArmed"].get<bool>();
        if (armed && !m_validationArmed) {
            // Re-arming clears per-source verdicts for a fresh comparison.
            m_packet.verified = false;
            m_packet.failed = false;
            m_object.verified = false;
            m_object.failed = false;
            m_validationPassed = false;
        }
        m_validationArmed = armed;
    }
    if (j.contains("validateExpectedSeed") && j["validateExpectedSeed"].is_number_integer()) {
        m_expectedSeed = j["validateExpectedSeed"].get<std::int64_t>();
        m_hasExpectedSeed = true;
    }
}

void SeedModule::saveConfig(nlohmann::json& j) {
    Module::saveConfig(j);
    j["hudPosX"] = m_hudPosX;
    j["hudPosY"] = m_hudPosY;
    j["isHudModule"] = m_isHudModule;
    j["m_showHud"] = m_showHud;
    j["m_textSize"] = m_textSize;
    j["validationArmed"] = m_validationArmed;
    // Always written so the menu entry persists; harmless while disarmed.
    j["validateExpectedSeed"] = m_hasExpectedSeed ? m_expectedSeed : 0;
}

}
