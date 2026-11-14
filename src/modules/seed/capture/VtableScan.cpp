#include "VtableScan.hpp"

#include <seedtaker/sdk/offsets/Seed.hpp>
#include <pl/memory/Vtable.hpp>
#include <link.h>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace seedtaker::capture {
namespace {

std::uint64_t load64(const void* p) {
    std::uint64_t value = 0;
    std::memcpy(&value, p, sizeof(value));
    return value;
}

struct ScanContext {
    const char* library;
    std::uint64_t slotA;
    std::uint64_t slotB;
    std::size_t firstSlot;
    std::uintptr_t base = 0;
};

int visitPhdr(dl_phdr_info* info, std::size_t, void* data) {
    auto* ctx = static_cast<ScanContext*>(data);
    if (!info->dlpi_name || !std::strstr(info->dlpi_name, ctx->library)) return 0;
    for (std::uint16_t i = 0; i < info->dlpi_phnum; ++i) {
        const auto& ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_LOAD) continue;
        if (!(ph.p_flags & PF_R) || (ph.p_flags & PF_X)) continue;
        const auto begin = static_cast<std::uintptr_t>(info->dlpi_addr + ph.p_vaddr);
        const auto end = begin + ph.p_memsz;
        if (end < begin + 16) continue;
        // Vtable slots are 8-aligned; both slots must match adjacently.
        for (auto p = begin; p + 16 <= end; p += 8) {
            if (load64(reinterpret_cast<const void*>(p)) == ctx->slotA
                && load64(reinterpret_cast<const void*>(p + 8)) == ctx->slotB) {
                ctx->base = p - ctx->firstSlot * 8;
                return 1;
            }
        }
    }
    return 0;
}

// 0 = unresolved, 1 = unresolvable, otherwise the verified base.
std::atomic<std::uintptr_t> levelDataBase{0};

}

std::uintptr_t resolveVtableBase(const char* rttiName, std::size_t firstSlot, const char* libraryName) {
    if (!rttiName || !libraryName) return 0;
    const auto addrA = pl::memory::resolveVtableFunction(rttiName, firstSlot, libraryName);
    const auto addrB = pl::memory::resolveVtableFunction(rttiName, firstSlot + 1, libraryName);
    if (!addrA || !addrB) return 0;
    ScanContext ctx{libraryName, addrA, addrB, firstSlot, 0};
    dl_iterate_phdr(visitPhdr, &ctx);
    return ctx.base;
}

bool isLevelDataInstance(const void* instance) {
    if (!instance) return false;
    auto base = levelDataBase.load(std::memory_order_acquire);
    if (base == 0) {
        base = resolveVtableBase(sdk::offsets::seed::LevelDataRtti, sdk::offsets::seed::LevelDataSeedSlot);
        levelDataBase.store(base ? base : 1, std::memory_order_release);
        if (!base) return false;
    } else if (base == 1) {
        return false;
    }
    return load64(instance) == base;
}

}
