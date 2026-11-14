#pragma once

// Byte-signature and vtable-slot resolver interface. Pattern adapted from
// BedrockTools (reference: BedrockTools/include/bedrocktools/memory/Signatures.hpp,
// resolved via preloader pl::memory::resolveSignatures / resolveVtableFunction).
//
// Status: PARTIAL — StartGameHandle is evidence-backed for MCPE 1.26.62
// (arm64); LevelInit is declared but undefined (pending ST-009). Entries are
// added only after Zaphkiel binary evidence (see docs/EVIDENCE_LOG.md).
// Do NOT copy patterns from BedrockTools: they target a different game build.

#include <seedtaker/Export.hpp>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace seedtaker::memory {

enum class SignatureId : std::uint16_t {
    // LegacyClientNetworkHandler slot 37 (F-028/F-033, ST-004/005).
    StartGameHandle,
    // Level::initialize address (pending ST-009; resolves to 0 until then).
    LevelInit,
    Count
};

inline constexpr std::size_t SignatureCount = static_cast<std::size_t>(SignatureId::Count);

struct SignatureDefinition {
    SignatureId id;
    std::string_view pattern;
    std::string_view rtti{};
    std::size_t vtableSlot = 0;
};

SEEDTAKER_API bool resolveAll(std::string_view libraryName = "libminecraftpe.so");
SEEDTAKER_API std::uintptr_t resolve(SignatureId id);
SEEDTAKER_API void clear();

}
