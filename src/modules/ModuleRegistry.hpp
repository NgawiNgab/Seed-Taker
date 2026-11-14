#pragma once

// Module registry: ownership, id lookup, lifecycle fan-out. Adapted from
// BedrockTools (reference: BedrockTools/src/modules/ModuleRegistry.hpp).
// Deviation: `using` aliases instead of PLModMenu_* macros.

#include "Module.hpp"
#include <pl/ModMenu.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace seedtaker {

using DrawCommand = pl::modmenu::DrawCommand;
using DrawCommandType = pl::modmenu::DrawCommandType;

inline void submitDrawCommands(std::string_view moduleId, const std::vector<DrawCommand>& commands) {
    pl::modmenu::submitDrawCommands(moduleId, commands);
}

class ModuleRegistry {
public:
    static ModuleRegistry& get();

    template <class T, class... Args>
    T& emplace(Args&&... args) {
        auto module = std::make_unique<T>(std::forward<Args>(args)...);
        auto* raw = module.get();
        mById.emplace(raw->moduleId, raw);
        mView.push_back(raw);
        mOwned.push_back(std::move(module));
        return *raw;
    }

    Module* find(std::string_view id) const;
    const std::vector<Module*>& modules() const;
    void initialize();
    void onFrame();
    bool onMouseEvent(int button, bool isDown);
    void setKeybindBlocked(bool blocked);
    bool keybindBlocked() const;

private:
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    std::vector<std::unique_ptr<Module>> mOwned;
    std::vector<Module*> mView;
    std::unordered_map<std::string, Module*, StringHash, std::equal_to<>> mById;
    bool mInitialized = false;
    bool mKeybindBlocked = false;
};

void registerAllModules();

}
