#pragma once
// Host-gate stub for vtable resolution (F-002). Returns an address, as the
// call sites assign it straight into std::uintptr_t.
#include <cstddef>
#include <cstdint>

namespace pl::memory {
std::uintptr_t resolveVtableFunction(const char* rttiName, std::size_t slot, const char* libraryName);
}
