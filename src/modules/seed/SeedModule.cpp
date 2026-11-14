#include "SeedModule.hpp"
#include "capture/SeedCapture.hpp"
#include "capture/SrcScan.hpp"
#include "modules/ModuleRegistry.hpp"
#include <pl/Logger.hpp>
#include <cstdio>
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
    capture::SrcScan::install();
}

void SeedModule::onDisable() {
    capture::SeedCapture::uninstall();
    capture::SrcScan::uninstall();
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
                m_packet.mismatchExpected = m_expectedSeed;
                m_packet.mismatchGot = packet.seed;
                pl::log::Logger::getOrCreate("SeedTaker")
                    .warn("Seed validation MISMATCH (packet): expected {}, got {}", m_expectedSeed, packet.seed);
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
                m_object.mismatchExpected = m_expectedSeed;
                m_object.mismatchGot = object.seed;
                pl::log::Logger::getOrCreate("SeedTaker")
                    .warn("Seed validation MISMATCH (object): expected {}, got {}", m_expectedSeed, object.seed);
            }
        }
    }

    // Setter source (F-088): the authoritative u64 seed. Unlike the src
    // windows it provides a value whenever the setter has fired, with or
    // without validation — so the seed displays on every load path.
    // Re-compared on EVERY new sequence (last call wins): on a network
    // load a placeholder can register before the server's real seed, so a
    // stale mismatch must never latch.
    const auto setter = capture::SeedCapture::setterReading();
    if (setter.sequence != 0 && setter.sequence != m_setter.sequence) {
        m_setter.sequence = setter.sequence;
        m_setter.value = setter.seed;
        // Frame thread, one line per registration (world load only): the
        // device log then shows how many times the setter fired and with
        // which values, which is exactly what decides the server question.
        pl::log::Logger::getOrCreate("SeedTaker")
            .info("Seed setter #{} registered {}", setter.sequence, setter.seed);
        if (m_validationArmed && m_hasExpectedSeed) {
            if (setter.seed == m_expectedSeed) {
                m_setter.verified = true;
                m_setter.failed = false;
                m_validationPassed = true;
            } else {
                m_setter.verified = false;
                m_setter.failed = true;
                pl::log::Logger::getOrCreate("SeedTaker")
                    .warn("Seed validation MISMATCH (setter): expected {}, got {}", m_expectedSeed, setter.seed);
            }
        }
    }

    for (std::size_t w = 0; w < sdk::offsets::seed::SrcScanWindowCount; ++w) {
        const auto snap = capture::SrcScan::windowReading(w);
        auto& st = m_srcWindows[w];
        if (snap.sequence != 0 && snap.sequence != st.sequence) {
            st.sequence = snap.sequence;
            st.value = snap.value;
            if (m_validationArmed && m_hasExpectedSeed && !st.latched) {
                st.latched = true;
                if (snap.value == m_expectedSeed) {
                    st.verified = true;
                    m_validationPassed = true;
                } else {
                    st.failed = true;
                    char off[16];
                    std::snprintf(off, sizeof(off), "0x%zx",
                                  sdk::offsets::seed::SrcScanWindows[w]);
                    pl::log::Logger::getOrCreate("SeedTaker")
                        .warn("Seed validation MISMATCH (src-scan {}): expected {}, got {}",
                              off, m_expectedSeed, snap.value);
                }
            }
        }
    }

    // Primary: a runtime-verified src window wins outright (only full-int64
    // leg); otherwise the setter seed (authoritative, F-088); otherwise the
    // newest usable packet/object reading (legacy int32 cross-checks).
    // Failed sources are excluded; divergences surface at render time,
    // never merged.
    const bool packetUsable = m_packet.sequence != 0 && !m_packet.failed;
    const bool objectUsable = m_object.sequence != 0 && !m_object.failed;
    const bool setterUsable = m_setter.sequence != 0 && !m_setter.failed;
    std::size_t verifiedSrc = sdk::offsets::seed::SrcScanWindowCount;
    std::size_t srcLatched = 0;
    std::size_t srcVerified = 0;
    for (std::size_t w = 0; w < sdk::offsets::seed::SrcScanWindowCount; ++w) {
        if (m_srcWindows[w].latched) ++srcLatched;
        if (m_srcWindows[w].verified) {
            ++srcVerified;
            if (verifiedSrc == sdk::offsets::seed::SrcScanWindowCount) verifiedSrc = w;
        }
    }
    m_reading.value.reset();
    m_reading.confidence = SeedConfidence::Unavailable;
    m_reading.source = "none";
    m_reading.detail = std::string(capture::SeedCapture::status()) + " | " + capture::SrcScan::status();
    if (capture::SeedCapture::setterHookActive() && m_setter.sequence == 0) {
        // An armed-but-silent setter is a distinct (and actionable) state:
        // the hook patched fine but no world-load registration has run yet.
        // Never let it read as "no hook" on a device report.
        m_reading.detail += " | setter: armed, no registration yet";
    }
    const SourceState* primary = nullptr;
    const char* primarySource = nullptr;
    if (verifiedSrc != sdk::offsets::seed::SrcScanWindowCount) {
        char label[64];
        std::snprintf(label, sizeof(label), "src-scan:src+0x%zx",
                      sdk::offsets::seed::SrcScanWindows[verifiedSrc]);
        m_reading.value = m_srcWindows[verifiedSrc].value;
        m_reading.source = label;
        m_reading.confidence = SeedConfidence::Recovered;
        m_reading.detail = "runtime-validated";
    } else if (setterUsable) {
        // Authoritative seed from the setter (F-088): the single
        // registration point, so this is the seed on every load path
        // (SP disk, LAN, Survival Server). Recovered when validated,
        // Inferred (but still primary) when not.
        m_reading.value = m_setter.value;
        m_reading.source = "level-setter:seed register";
        m_reading.confidence = m_setter.verified ? SeedConfidence::Recovered : SeedConfidence::Inferred;
        m_reading.detail = m_setter.verified ? "runtime-validated" : "authoritative seed (F-088)";
    } else if (packetUsable && (!objectUsable || m_packet.sequence >= m_object.sequence)) {
        primary = &m_packet;
        primarySource = "server-packet:StartGamePacket+0xA8";
    } else if (objectUsable) {
        primary = &m_object;
        primarySource = "level-object:LevelData+0x3b0";
    }
    if (m_reading.confidence == SeedConfidence::Unavailable && primary) {
        m_reading.value = static_cast<std::int64_t>(primary->value);
        m_reading.source = primarySource;
        if (primary->verified) {
            m_reading.confidence = SeedConfidence::Recovered;
            m_reading.detail = "runtime-validated";
        } else {
            m_reading.confidence = SeedConfidence::Inferred;
            m_reading.detail = "unconfirmed field (F-051)";
        }
    } else if (m_reading.confidence == SeedConfidence::Unavailable
               && (m_packet.failed || m_object.failed || m_setter.failed || srcLatched > srcVerified)) {
        std::string detail = "mismatch:";
        bool first = true;
        if (m_packet.failed) {
            detail += " pkt exp " + std::to_string(m_packet.mismatchExpected)
                + " got " + std::to_string(m_packet.mismatchGot);
            first = false;
        }
        if (m_object.failed) {
            if (!first) detail += ";";
            detail += " obj exp " + std::to_string(m_object.mismatchExpected)
                + " got " + std::to_string(m_object.mismatchGot);
            first = false;
        }
        if (m_setter.failed) {
            if (!first) detail += ";";
            detail += " setter exp " + std::to_string(m_expectedSeed)
                + " got " + std::to_string(m_setter.value);
            first = false;
        }
        if (srcLatched > 0) {
            if (!first) detail += ";";
            detail += " src " + std::to_string(srcVerified) + "/" + std::to_string(srcLatched);
        }
        m_reading.detail = detail;
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

    // Src-scan debug: while armed and unmatched, show every snapped window
    // so a field report carries the raw values (logcat is unreachable).
    if (m_validationArmed && !m_validationPassed) {
        for (std::size_t w = 0; w < sdk::offsets::seed::SrcScanWindowCount; ++w) {
            if (m_srcWindows[w].sequence == 0) continue;
            char line[64];
            std::snprintf(line, sizeof(line), "src+0x%zx = %lld",
                          sdk::offsets::seed::SrcScanWindows[w],
                          static_cast<long long>(m_srcWindows[w].value));
            DrawCommand win{};
            win.type = DrawCommandType::Text;
            win.x = m_hudPosX;
            win.y = y;
            win.w = 900.0f;
            win.h = m_textSize + 8.0f;
            win.color = 0xFF88FF88;
            win.size = m_textSize;
            win.text = line;
            commands.push_back(win);
            y += m_textSize + 8.0f;
        }
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
        // No int32-range caveat any more (F-088): the setter and the src
        // windows carry the seed as a full int64, so an expectation outside
        // int32 CAN match. Only the legacy packet/object cross-checks are
        // int32, and they no longer decide the verdict.
        commands.push_back(validation);
        y += m_textSize + 8.0f;
    }
}

void SeedModule::onFrame() {
    if (!enabled || !m_showHud) return;
    // Install is idempotent: retry until live (covers enable-before-load).
    if (!capture::SeedCapture::packetHookActive() || !capture::SeedCapture::setterHookActive()) {
        capture::SeedCapture::install();
    }
    if (!capture::SrcScan::active()) capture::SrcScan::install();
    drainCapture();
    std::vector<DrawCommand> commands;
    float y = m_hudPosY;
    renderReading(commands, y);
    submitDrawCommands(moduleId, commands);
}

bool SeedModule::onMenuConfigChanged(std::string_view key, std::string_view value) {
    if (key != "validateExpectedSeed") return false;
    // Full-int64 menu path: the generic ModuleMenu funnel parses integers
    // via strtol-into-int and would truncate seeds outside the 32-bit
    // range. Invalid input keeps the previous value; empty clears it.
    const std::string text(value);
    if (text.empty()) {
        m_hasExpectedSeed = false;
        return true;
    }
    char* end = nullptr;
    const long long parsed = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return true;
    m_expectedSeed = static_cast<std::int64_t>(parsed);
    m_hasExpectedSeed = true;
    return true;
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
            m_setter.verified = false;
            m_setter.failed = false;
            for (auto& st : m_srcWindows) {
                st.verified = false;
                st.failed = false;
                st.latched = false;
            }
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
