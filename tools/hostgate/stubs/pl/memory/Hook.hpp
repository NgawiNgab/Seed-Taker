#pragma once
// Host-gate stub for preloader hooking (F-002).
#include <cstddef>
#include <cstdint>

namespace pl::memory {
int hook(void* target, void* detour, void** original);
int unhook(void* target, void* detour);
}
