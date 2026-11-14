#include "ModuleMenu.hpp"
#include "modules/ModuleRegistry.hpp"
#include "core/Runtime.hpp"
#include "config/ConfigManager.hpp"
#include <pl/ModMenu.hpp>
#include <pl/Logger.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace seedtaker::launcher {
namespace {

void onModuleToggle(std::string_view module_id, bool enabled) {
    auto* mod = ModuleRegistry::get().find(module_id);
    if (!mod) return;
    mod->setMasterEnabled(enabled);
    seedtaker::config::ConfigManager::get().save();
}

void onModuleKeybind(std::string_view module_id, std::string_view key, bool isDown) {
    if (isDown && ModuleRegistry::get().keybindBlocked()) return;
    auto* mod = ModuleRegistry::get().find(module_id);
    if (!mod) return;
    mod->onKeybindEvent(std::string(key), isDown);
}

void onModuleConfigChanged(std::string_view module_id, std::string_view key, std::string_view value) {
    auto* mod = ModuleRegistry::get().find(module_id);
    if (!mod) return;
    if (mod->onMenuConfigChanged(key, value)) {
        seedtaker::config::ConfigManager::get().save();
        return;
    }

    nlohmann::json j;
    mod->saveConfig(j);

    std::string safeValue(value);
    std::string safeKey(key);
    try {
        if (j.contains(safeKey)) {
            if (j[safeKey].is_boolean()) {
                if (safeValue == "true") j[safeKey] = true;
                else if (safeValue == "false") j[safeKey] = false;
            } else if (j[safeKey].is_number_integer()) {
                if (!safeValue.empty()) {
                    char* end;
                    int val = std::strtol(safeValue.c_str(), &end, 10);
                    if (end != safeValue.c_str()) j[safeKey] = val;
                }
            } else if (j[safeKey].is_number_float()) {
                if (!safeValue.empty()) {
                    char* end;
                    float val = std::strtof(safeValue.c_str(), &end);
                    if (end != safeValue.c_str()) j[safeKey] = val;
                }
            } else {
                j[safeKey] = safeValue;
            }
        } else {
            j[safeKey] = safeValue;
        }
    } catch (...) {
        j[safeKey] = safeValue;
    }
    mod->loadConfig(j);
    seedtaker::config::ConfigManager::get().save();
}

} // namespace

// Menu registration adapted from BedrockTools
// (reference: BedrockTools/src/launcher/ModuleMenu.cpp). Trimmed to generic
// type inference: BedrockTools' per-module slider heuristics (Motion Blur,
// CPS, FOV, hand transforms, ...) are not applicable here.
void registerModulesWithLauncher() {
    auto& modules = ModuleRegistry::get().modules();

    for (auto* mod : modules) {
        if (!mod->showInMenu) continue;

        pl::modmenu::ModuleBuilder builder(mod->moduleId, mod->name);
        builder.description(mod->description)
                .modId(seedtaker::core::Runtime::get().modId())
                .defaultEnabled(mod->masterEnabled)
                .hideInHudEditor(mod->hideInHudEditor)
                .onToggle(onModuleToggle)
                .onConfigChanged(onModuleConfigChanged)
                .onKeybind(onModuleKeybind);

        nlohmann::json j;
        mod->saveConfig(j);

        static const char* baseKeys[] = {
                "masterEnabled", "keybindActive", "keybind",
                "isHudModule", "hudPosX", "hudPosY"
        };

        struct TmpConfigEntry {
            std::string key;
            std::string displayName;
            pl::modmenu::ConfigType type;
            std::string default_value;
            std::string min_value;
            std::string max_value;
            std::string depends_on;
        };
        std::vector<TmpConfigEntry> configs;

        for (auto& [k, v] : j.items()) {
            bool isBase = false;
            for (const char* bk : baseKeys) {
                if (k == bk) { isBase = true; break; }
            }
            std::string keyLower = k;
            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
            const bool isHudPosition = keyLower.rfind("hud", 0) == 0 &&
                (keyLower.ends_with("posx") || keyLower.ends_with("posy"));
            if (isBase || isHudPosition || !mod->showInLegacyMenu(k)) continue;

            std::string displayName;
            std::string sourceKey = k;
            if (sourceKey.size() > 2 && sourceKey[0] == 'm' && sourceKey[1] == '_') {
                sourceKey = sourceKey.substr(2);
            }
            for (size_t i = 0; i < sourceKey.size(); ++i) {
                if (i == 0) {
                    displayName += toupper(sourceKey[i]);
                } else if (isupper(sourceKey[i])) {
                    displayName += ' ';
                    displayName += sourceKey[i];
                } else {
                    displayName += sourceKey[i];
                }
            }

            TmpConfigEntry entry;
            entry.key = k;
            entry.displayName = displayName;

            // Deviation from generic inference (single key): a seed spans
            // the full int64 range, which the default 0..200 SliderInt
            // cannot represent — free-form text with menu-side parsing in
            // SeedModule::onMenuConfigChanged.
            if (k == "validateExpectedSeed" && v.is_number_integer()) {
                entry.type = pl::modmenu::ConfigType::Text;
                entry.default_value = std::to_string(v.get<std::int64_t>());
            } else if (v.is_boolean()) {
                std::string kLower = k;
                std::transform(kLower.begin(), kLower.end(), kLower.begin(), ::tolower);
                if (kLower.find("button") != std::string::npos) {
                    entry.type = pl::modmenu::ConfigType::Button;
                } else {
                    entry.type = pl::modmenu::ConfigType::Toggle;
                }
                entry.default_value = v.get<bool>() ? "true" : "false";
            } else if (v.is_number_integer()) {
                entry.type = pl::modmenu::ConfigType::SliderInt;
                entry.default_value = std::to_string(v.get<int>());

                std::string kLower = k;
                std::transform(kLower.begin(), kLower.end(), kLower.begin(), ::tolower);

                if (kLower.find("keybind") != std::string::npos) {
                    entry.type = pl::modmenu::ConfigType::Keybind;
                } else {
                    entry.min_value = std::to_string(0);
                    entry.max_value = std::to_string(200);
                }
            } else if (v.is_number_float()) {
                entry.type = pl::modmenu::ConfigType::SliderFloat;
                entry.default_value = std::to_string(v.get<float>());
                entry.min_value = std::to_string(0.0f);
                entry.max_value = std::to_string(100.0f);
            } else if (v.is_string()) {
                std::string str = v.get<std::string>();
                if (str.find(',') != std::string::npos) {
                    entry.type = pl::modmenu::ConfigType::Radio;
                    size_t firstComma = str.find(',');
                    entry.default_value = str.substr(0, firstComma);
                    entry.min_value = str.substr(firstComma + 1);
                } else if (!str.empty() && str[0] == '#') {
                    entry.type = pl::modmenu::ConfigType::Color;
                    entry.default_value = str;
                } else {
                    entry.type = pl::modmenu::ConfigType::Text;
                    entry.default_value = str;
                }
            } else {
                continue;
            }
            configs.push_back(entry);
        }

        for (auto& entry : configs) {
            std::string k = entry.key;
            if (entry.type != pl::modmenu::ConfigType::Toggle) {
                std::string bestParent = "";
                for (const auto& parentCandidate : configs) {
                    if (parentCandidate.type == pl::modmenu::ConfigType::Toggle) {
                        std::string pKey = parentCandidate.key;
                        if (k.length() > pKey.length() && k.compare(0, pKey.length(), pKey) == 0) {
                            if (pKey.length() > bestParent.length()) {
                                bestParent = pKey;
                            }
                        }
                    }
                }
                if (!bestParent.empty()) entry.depends_on = bestParent;
            }
            builder.config(entry.key, entry.displayName, entry.type, entry.default_value, entry.min_value, entry.max_value, entry.depends_on);
        }

        if (builder.registerModule()) {
            mod->onMenuRegistered();
        } else {
            pl::log::Logger::getOrCreate("SeedTaker").error("Module registration failed");
        }
    }
}

}
