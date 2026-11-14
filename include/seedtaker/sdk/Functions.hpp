#pragma once

// Resolved-address to function-pointer helper. Adapted from the BedrockTools
// SDK pattern (reference: BedrockTools/include/bedrocktools/sdk/Functions.hpp).
// Deviation: no runtime-API indirection — Seed-Taker is standalone and reads
// its own signature table directly.

#include <seedtaker/memory/Signatures.hpp>

namespace seedtaker::sdk {

template <class Function>
Function function(memory::SignatureId id) {
    const auto address = memory::resolve(id);
    return address ? reinterpret_cast<Function>(address) : nullptr;
}

}
