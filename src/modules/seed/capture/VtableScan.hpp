#pragma once

// Runtime vtable-base resolver. Type safety for vtable calls on
// game-provided instances (F-055): before calling a slot on an object whose
// dynamic type is only inferred, the instance's vtable pointer is compared
// against a verified vtable base. The base is located by finding two
// adjacent RTTI-resolved slots in the library mapping, so a single stale
// address can never validate a wrong object. Resolution runs at most once
// per process; failures permanently disable the dependent path (the capture
// reports unavailable, never guesses).

#include <cstddef>
#include <cstdint>

namespace seedtaker::capture {

// Returns the primary vtable base for `rttiName` (slot `firstSlot` is
// verified adjacent to `firstSlot + 1`), or 0 when unverifiable.
std::uintptr_t resolveVtableBase(const char* rttiName, std::size_t firstSlot,
                                 const char* libraryName = "libminecraftpe.so");

// True when `instance` is non-null and its vtable pointer equals the
// verified LevelData vtable base. False covers both "not LevelData" and
// "base unverifiable" — callers must treat both as "do not call".
bool isLevelDataInstance(const void* instance);

}
