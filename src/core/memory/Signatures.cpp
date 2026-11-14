#include <seedtaker/memory/Signatures.hpp>

#include <seedtaker/sdk/offsets/Seed.hpp>
#include <array>
#include <string>
#include <vector>
#include <pl/memory/Signature.hpp>
#include <pl/memory/Vtable.hpp>

namespace seedtaker::memory {
namespace {
std::array<std::uintptr_t, SignatureCount> addresses{};
// RTTI+vtable resolution only — no byte patterns (F-052). LevelInit stays
// undefined (pending ST-009) so it resolves to 0 and its hook stays
// disabled; defining it later activates the object path with no code change.
const std::array<SignatureDefinition, SignatureCount> definitions{{
    SignatureDefinition{SignatureId::StartGameHandle,
                        {},
                        sdk::offsets::seed::HandlerRtti,
                        sdk::offsets::seed::HandlerStartGameSlot},
    SignatureDefinition{SignatureId::LevelInit, {}, {}, 0},
}};
}

bool resolveAll(std::string_view libraryName) {
    const std::string library(libraryName);
    std::vector<std::string> patterns;
    patterns.reserve(definitions.size());
    for (const auto& definition : definitions) {
        if (!definition.pattern.empty()) patterns.emplace_back(definition.pattern);
    }
    const auto resolved = pl::memory::resolveSignatures(patterns, library.c_str());
    addresses.fill(0);
    bool any = false;
    for (const auto& definition : definitions) {
        std::uintptr_t address = 0;
        if (!definition.rtti.empty()) {
            const std::string rtti(definition.rtti);
            address = pl::memory::resolveVtableFunction(rtti.c_str(), definition.vtableSlot, library.c_str());
        } else if (!definition.pattern.empty()) {
            const auto it = resolved.find(std::string(definition.pattern));
            if (it != resolved.end()) address = it->second;
        }
        addresses[static_cast<std::size_t>(definition.id)] = address;
        any = any || address != 0;
    }
    return any;
}

std::uintptr_t resolve(SignatureId id) {
    const auto index = static_cast<std::size_t>(id);
    return index < addresses.size() ? addresses[index] : 0;
}

void clear() {
    addresses.fill(0);
}

}
