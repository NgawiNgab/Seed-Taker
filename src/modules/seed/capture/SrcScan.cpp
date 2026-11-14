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
seedtaker::hooks::Handle ctorHook = nullptr;
void* originalCtor = nullptr;

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

}

// Entry hook: runs inside the ASM wrapper with (this, src); returns the
// original ctor address for the tail-call. No locks, no allocs, no logs.
extern "C" void* SeedTaker_SrcCtorEntry(const void* this_, const void* src) {
    (void)this_;
    if (src) snapshotSrc(src);
    return originalCtor;
}

#if defined(__aarch64__)
// Preserve-all wrapper (F-074). The ctor's full signature is unknown, so
// instead of declaring it, the wrapper saves the entire argument regfile
// (x0-x8, d0-d7), runs the entry hook, restores everything, and tail-calls
// the original. Post-call the original's residue is left untouched —
// bit-identical to unhooked behavior. Audit: x9+ are never arg regs; d8+
// preserved by the C call per ABI; q-upper halves safe (float params are
// ≤ s1); x18 platform-preserved; flags are entry-independent; sp stays
// 16-aligned. No unwind info by design (tombstones stop here — fine).
__asm__(
".text\n"
".p2align 2\n"
".global SeedTaker_SrcCtorTrampoline\n"
".type SeedTaker_SrcCtorTrampoline, %function\n"
"SeedTaker_SrcCtorTrampoline:\n"
"    stp x29, x30, [sp, #-0xA0]!\n"
"    mov x29, sp\n"
"    stp x0, x1, [sp, #0x10]\n"
"    stp x2, x3, [sp, #0x20]\n"
"    stp x4, x5, [sp, #0x30]\n"
"    stp x6, x7, [sp, #0x40]\n"
"    str x8, [sp, #0x50]\n"
"    stp d0, d1, [sp, #0x58]\n"
"    stp d2, d3, [sp, #0x68]\n"
"    stp d4, d5, [sp, #0x78]\n"
"    stp d6, d7, [sp, #0x88]\n"
"    ldp x0, x1, [sp, #0x10]\n"
"    bl SeedTaker_SrcCtorEntry\n"
"    mov x16, x0\n"
"    ldp x0, x1, [sp, #0x10]\n"
"    ldp x2, x3, [sp, #0x20]\n"
"    ldp x4, x5, [sp, #0x30]\n"
"    ldp x6, x7, [sp, #0x40]\n"
"    ldr x8, [sp, #0x50]\n"
"    ldp d0, d1, [sp, #0x58]\n"
"    ldp d2, d3, [sp, #0x68]\n"
"    ldp d4, d5, [sp, #0x78]\n"
"    ldp d6, d7, [sp, #0x88]\n"
"    blr x16\n"
"    ldp x29, x30, [sp], #0xA0\n"
"    ret\n"
".size SeedTaker_SrcCtorTrampoline, .-SeedTaker_SrcCtorTrampoline\n");
extern "C" void SeedTaker_SrcCtorTrampoline();
#endif

bool SrcScan::install() {
    std::lock_guard lock(installMutex);
    if (ctorHook) return true;
#if !defined(__aarch64__)
    statusText.store("src-scan: arm64 only", std::memory_order_release);
    return false;
#else
    const auto libBase = libraryBase("libminecraftpe.so");
    if (!libBase) {
        statusText.store("src-scan: unresolved", std::memory_order_release);
        return false;
    }
    std::uint32_t anchor1 = 0;
    std::uint32_t anchor2 = 0;
    std::memcpy(&anchor1,
                reinterpret_cast<const void*>(libBase + seed::LevelDataCtorBlSite1),
                sizeof(anchor1));
    std::memcpy(&anchor2,
                reinterpret_cast<const void*>(libBase + seed::LevelDataCtorBlSite2),
                sizeof(anchor2));
    if (anchor1 != seed::LevelDataCtorBlWord1 || anchor2 != seed::LevelDataCtorBlWord2) {
        statusText.store("src-scan: anchor mismatch", std::memory_order_release);
        return false;
    }
    ctorHook = seedtaker::hooks::install(
        reinterpret_cast<void*>(libBase + seed::LevelDataCtorFileVA),
        reinterpret_cast<void*>(SeedTaker_SrcCtorTrampoline),
        reinterpret_cast<void**>(&originalCtor));
    statusText.store(ctorHook ? "src-scan: active" : "src-scan: hook failed",
                     std::memory_order_release);
    return ctorHook != nullptr;
#endif
}

void SrcScan::uninstall() {
    std::lock_guard lock(installMutex);
    if (ctorHook) {
        seedtaker::hooks::remove(ctorHook);
        ctorHook = nullptr;
    }
    originalCtor = nullptr;
    statusText.store("src-scan: idle", std::memory_order_release);
}

bool SrcScan::active() {
    return ctorHook != nullptr;
}

SrcWindowReading SrcScan::windowReading(std::size_t window) {
    if (window >= seed::SrcScanWindowCount) return {};
    // Seq-guarded pair: retry while a snapshot lands mid-read (bounded;
    // the ctor fires rarely, so the first try almost always wins).
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

} // namespace seedtaker::capture
