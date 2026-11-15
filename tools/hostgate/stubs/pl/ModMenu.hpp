#pragma once
// Host-gate stub for the launcher menu surface used by ModuleMenu.cpp.
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace pl::modmenu {
enum class ConfigType { Toggle, SliderInt, SliderFloat, Text, Button, Keybind, Radio, Color };

enum class DrawCommandType { Text, Rect, Sprite };

struct DrawCommand {
    DrawCommandType type = DrawCommandType::Text;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::uint32_t color = 0;
    float size = 0.0f;
    std::string text;
};

// UNRESOLVED SHAPE: the real preloader signature is not recorded in the
// evidence log. A variadic template accepts whatever the wrapper passes and
// loses to the non-template seedtaker::submitDrawCommands, so it never
// manufactures an ADL ambiguity that the real build does not have (CI,
// which links the real preloader, is the authority on the true signature).
template <class... Args>
void submitDrawCommands(Args&&...);

class ModuleBuilder {
public:
    ModuleBuilder(const std::string& id, const std::string& name) {
        (void)id;
        (void)name;
    }
    ModuleBuilder& description(const std::string&) { return *this; }
    ModuleBuilder& modId(const std::string&) { return *this; }
    ModuleBuilder& defaultEnabled(bool) { return *this; }
    ModuleBuilder& hideInHudEditor(bool) { return *this; }
    ModuleBuilder& onToggle(std::function<void(std::string_view, bool)>) { return *this; }
    ModuleBuilder& onConfigChanged(std::function<void(std::string_view, std::string_view, std::string_view)>) { return *this; }
    ModuleBuilder& onKeybind(std::function<void(std::string_view, std::string_view, bool)>) { return *this; }
    ModuleBuilder& config(const std::string&, const std::string&, ConfigType,
                          const std::string&, const std::string&,
                          const std::string&, const std::string&) { return *this; }
    bool registerModule() { return false; }
};
}
