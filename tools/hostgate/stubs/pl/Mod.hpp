#pragma once
// Host-gate stub for pl::mod::ModContext (F-002).
#include <filesystem>
#include <string>

namespace pl::mod {
class ModContext {
public:
    std::string id() const { return {}; }

// Host-gate stand-in for PL_REGISTER_MOD: the real macro expands to preloader
// registration boilerplate that needs the NDK toolchain.
#define PL_REGISTER_MOD(ModType, instance) \
    extern "C" bool seedtaker_hostgate_mod_stub() { (void)sizeof(ModType); return true; }
    std::filesystem::path resourceDir() const { return {}; }

// Host-gate stand-in for PL_REGISTER_MOD: the real macro expands to preloader
// registration boilerplate that needs the NDK toolchain.
#define PL_REGISTER_MOD(ModType, instance) \
    extern "C" bool seedtaker_hostgate_mod_stub() { (void)sizeof(ModType); return true; }
    std::filesystem::path configDir() const { return {}; }

// Host-gate stand-in for PL_REGISTER_MOD: the real macro expands to preloader
// registration boilerplate that needs the NDK toolchain.
#define PL_REGISTER_MOD(ModType, instance) \
    extern "C" bool seedtaker_hostgate_mod_stub() { (void)sizeof(ModType); return true; }
};
}

// Host-gate stand-in for PL_REGISTER_MOD: the real macro expands to preloader
// registration boilerplate that needs the NDK toolchain.
#define PL_REGISTER_MOD(ModType, instance) \
    extern "C" bool seedtaker_hostgate_mod_stub() { (void)sizeof(ModType); return true; }
