#include "SrcScan.hpp"
#include "VtableScan.hpp"

#include "core/memory/Hooks.hpp"
#include <seedtaker/sdk/offsets/Seed.hpp>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace seedtaker::capture {
namespace {
namespace seed = seedtaker::sdk::offsets::seed;

std::atomic<std::int64_t> windowValues[seed::SrcScanWindowCount]{};
std::atomic<std::uint64_t> windowSeqs[seed::SrcScanWindowCount]{};
std::atomic<const char*> statusText{"src-scan: idle"};
std::mutex installMutex;
seedtaker::hooks::Handle getterHook = nullptr;

// int32 (this) — the trivial getter's exact shape (F-048), so the detour
// needs no signature guesswork.
using SrcGetter_t = std::int32_t (*)(const void* src);
SrcGetter_t originalGetter = nullptr;

void snapshotSrc(const void* src) {
    const auto base = reinterpret_cast<std::uintptr_t>(src);
    for (std::size_t i = 0; i < seed::SrcScanWindowCount; ++i) {
        std::int64_t value = 0;
        std::memcpy(&value,
                    reinterpret_cast<const void*>(base + seed::SrcScanWindows[i]),
                    sizeof(value));
        windowValues[i].store(value, std::memory_order_relaxed);
        windowSeqs[i].fetch_add(1, std::memory_order_release);
    }
}

std::int32_t srcGetterDetour(const void* src) {
    std::int32_t result = 0;
    if (originalGetter) result = originalGetter(src);
    // Post-call snapshot: the getter just read src+0x78, so the cluster
    // mapping is proven fresh on every invocation (F-071/F-072).
    if (src) snapshotSrc(src);
    return result;
}

}

bool SrcScan::install() {
    std::lock_guard lock(installMutex);
    if (getterHook) return true;
    const auto libBase = libraryBase("libminecraftpe.so");
    if (!libBase) {
        statusText.store("src-scan: unresolved", std::memory_order_release);
        return false;
    }
    const auto address = libBase + seed::SrcGetterFileVA;
    std::uint64_t body = 0;
    std::memcpy(&body, reinterpret_cast<const void*>(address), sizeof(body));
    if (body != seed::SrcGetterBody) {
        statusText.store("src-scan: body mismatch", std::memory_order_release);
        return false;
    }
    getterHook = seedtaker::hooks::install(reinterpret_cast<void*>(address),
                                            reinterpret_cast<void*>(srcGetterDetour),
                                            reinterpret_cast<void**>(&originalGetter));
    statusText.store(getterHook ? "src-scan: active" : "src-scan: hook failed",
                     std::memory_order_release);
    return getterHook != nullptr;
}

void SrcScan::uninstall() {
    std::lock_guard lock(installMutex);
    if (getterHook) {
        seedtaker::hooks::remove(getterHook);
        getterHook = nullptr;
    }
    originalGetter = nullptr;
    statusText.store("src-scan: idle", std::memory_order_release);
}

bool SrcScan::active() {
    return getterHook != nullptr;
}

SrcWindowReading SrcScan::windowReading(std::size_t window) {
    if (window >= seed::SrcScanWindowCount) return {};
    // Seq-guarded pair: retry while a snapshot lands mid-read (bounded;
    // the getter fires rarely, so the first try almost always wins).
    for (int attempt = 0; attempt < 4; ++attempt) {
        const auto before = windowSeqs[window].load(std::memory_order_acquire);
        const auto value = windowValues[window].load(std::memory_order_relaxed);
        const auto after = windowSeqs[window].load(std::memory_order_acquire);
        if (before == after) return SrcWindowReading{before, value};
    }
    return SrcWindowReading{windowSeqs[window].load(std::memory_order_acquire),
                            windowValues[window].load(std::memory_order_relaxed)};
}

const char* SrcScan::status() {
    return statusText.load(std::memory_order_acquire);
}

}
