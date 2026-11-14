#include "SeedCapture.hpp"
#include "VtableScan.hpp"

#include "core/memory/Hooks.hpp"
#include <seedtaker/memory/Signatures.hpp>
#include <seedtaker/sdk/Memory.hpp>
#include <seedtaker/sdk/offsets/Seed.hpp>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string> // Pointer types only; the detour never dereferences these.

namespace seedtaker::capture {
namespace {

std::atomic<std::uint64_t> packetPacked{0};
std::atomic<std::uint64_t> objectPacked{0};
std::atomic<std::uint64_t> rejectedCount{0};
std::atomic<std::uint64_t> mismatchCount{0};
std::atomic<const char*> statusText{"capture: idle"};
std::mutex installMutex;
seedtaker::hooks::Handle packetHook = nullptr;
seedtaker::hooks::Handle levelInitHook = nullptr;
seedtaker::hooks::Handle setterHook = nullptr;

// Setter source (F-088): seqlock. setterSeq is odd while a write is in
// flight, so readers retry instead of observing a torn 64-bit value.
std::atomic<std::uint64_t> setterSeq{0};
std::atomic<std::int64_t> setterValue{0};

// Why the setter hook is (or is not) live — surfaced in status() so a
// device report distinguishes "not attempted" from "anchor mismatch"
// (the latter means the image is not the one the offsets came from).
enum SetterHookState : int {
    SetterHookNotAttempted = 0,
    SetterHookUnresolved,   // library base not found.
    SetterHookAnchorBad,    // bl anchors do not match: wrong image, refused.
    SetterHookInstallFail,  // anchors fine, hooks::install refused.
    SetterHookLive,
};
std::atomic<int> setterState{SetterHookNotAttempted};

// handle(NetworkIdentifier const&, StartGamePacket const&) — x0/x1/x2 per
// the dispatcher tail-call (F-028). Return-type assumption (F-055): the
// handler returns void; the original call is the detour's LAST statement so
// any result register flows through untouched regardless.
using StartGameHandle_t = void (*)(void* handler, const void* networkId, const void* packet);
StartGameHandle_t originalHandle = nullptr;

// Level::initialize — EXACT BedrockTools signature, re-verified 2026-11-14
// against BedrockTools src/modules/hud/debugmenu.cpp:92-99 (F-053).
// levelSettings is received opaquely (never type-assumed); the detour
// validates it with isLevelDataInstance() before any vtable call (F-055).
using LevelInitialize_t = bool (*)(void* _this,
                                   const std::string* levelName,
                                   void* levelSettings,
                                   void* experiments,
                                   const std::string* levelId,
                                   void* biomeOptLo,
                                   void* biomeOptHi);
LevelInitialize_t originalLevelInit = nullptr;

// LevelData seed setter (F-086): registers the u64 seed as a type-7
// gamerule. Called as setter(this, seed) — the seed arrives in x1
// (AAPCS64, confirmed by the ctor's setter(this, ab28(src)) call site).
// Return-type assumption (F-055): the original call is the detour's LAST
// statement so any result register flows through untouched regardless.
using LevelDataSeedSetter_t = void (*)(void* _this, std::int64_t seed);
LevelDataSeedSetter_t originalSetter = nullptr;

void publishPacket(std::int32_t seed) {
    const auto previous = packetPacked.load(std::memory_order_relaxed);
    const auto sequence = (previous >> 32) + 1;
    packetPacked.store((sequence << 32) | static_cast<std::uint32_t>(seed), std::memory_order_release);
}

void publishObject(std::int32_t seed) {
    const auto previous = objectPacked.load(std::memory_order_relaxed);
    const auto sequence = (previous >> 32) + 1;
    objectPacked.store((sequence << 32) | static_cast<std::uint32_t>(seed), std::memory_order_release);
}

// Seqlock publish (F-088): odd sequence = write in flight. Release stores
// so the value is visible before the settle. No allocation, no locks.
void publishSetter(std::int64_t seed) {
    const auto begin = setterSeq.load(std::memory_order_relaxed);
    setterSeq.store(begin + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    setterValue.store(seed, std::memory_order_relaxed);
    setterSeq.store(begin + 2, std::memory_order_release);
}

void startGameHandleDetour(void* handler, const void* networkId, const void* packet) {
    if (packet && originalHandle) {
        // Self-gate: slot 37 is trusted for StartGame only (F-027/F-055).
        const int packetId = sdk::virtualCall<int>(const_cast<void*>(packet), sdk::offsets::seed::StartGameIdSlot);
        if (packetId == sdk::offsets::seed::StartGamePacketId) {
            publishPacket(sdk::field<const std::int32_t>(packet, sdk::offsets::seed::StartGameSeedOffset));
        } else {
            rejectedCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (originalHandle) originalHandle(handler, networkId, packet);
}

bool levelInitializeDetour(void* _this,
                           const std::string* levelName,
                           void* levelSettings,
                           void* experiments,
                           const std::string* levelId,
                           void* biomeOptLo,
                           void* biomeOptHi) {
    if (levelSettings && originalLevelInit) {
        if (isLevelDataInstance(levelSettings)) {
            const auto seed = sdk::virtualCall<std::int32_t>(levelSettings, sdk::offsets::seed::LevelDataSeedSlot);
            publishObject(seed);
        } else {
            mismatchCount.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (originalLevelInit) {
        return originalLevelInit(_this, levelName, levelSettings, experiments, levelId, biomeOptLo, biomeOptHi);
    }
    return false;
}

// The seed setter is the single registration point for the world seed
// (F-086): every load path — SP disk, LAN, Survival Server packet —
// funnels the u64 seed through it. Capturing x1 here yields the
// authoritative seed regardless of how the world was loaded (F-088).
// The setter fires only when a seed is registered (world load), never
// per-frame, so the detour cost is negligible. Last call wins: on a
// network load the ctor can register a placeholder before the server's
// real seed is applied, and the later (real) registration overwrites it
// — SeedModule revalidates on every new sequence so a corrected value is
// never stuck behind a stale mismatch.
void setterDetour(void* _this, std::int64_t seed) {
    publishSetter(seed);
    if (originalSetter) originalSetter(_this, seed);
}

// Install the setter hook (F-088). The target is resolved by sliding the
// file VA with the library base (ET_DYN) and then verifying TWO `bl`
// words inside the function before patching: the anchor formula was
// script-computed and cross-checked against the known ctor anchor
// (0x97E75B05), so a mismatch means the image is not the one the offsets
// were derived from — refuse rather than patch the wrong function.
bool installSetterHook() {
    // Terminal states stop the per-frame retry loop dead: an anchor
    // mismatch or an install refusal is deterministic, and re-resolving
    // the library every frame is pure waste. Only "not yet attempted" and
    // "library not loaded" retry.
    const auto state = setterState.load(std::memory_order_acquire);
    if (state == SetterHookAnchorBad || state == SetterHookInstallFail) return false;
    const auto base = libraryBase("libminecraftpe.so");
    if (!base) {
        setterState.store(SetterHookUnresolved, std::memory_order_release);
        return false;
    }
    const auto target = base + sdk::offsets::seed::LevelDataSeedSetterFileVA;
    std::uint32_t anchor1 = 0;
    std::uint32_t anchor2 = 0;
    std::memcpy(&anchor1,
                reinterpret_cast<const void*>(base + sdk::offsets::seed::LevelDataSeedSetterBlSite1),
                sizeof(anchor1));
    std::memcpy(&anchor2,
                reinterpret_cast<const void*>(base + sdk::offsets::seed::LevelDataSeedSetterBlSite2),
                sizeof(anchor2));
    if (anchor1 != sdk::offsets::seed::LevelDataSeedSetterBlWord1
        || anchor2 != sdk::offsets::seed::LevelDataSeedSetterBlWord2) {
        setterState.store(SetterHookAnchorBad, std::memory_order_release);
        return false;
    }
    setterHook = seedtaker::hooks::install(reinterpret_cast<void*>(target),
                                           reinterpret_cast<void*>(setterDetour),
                                           reinterpret_cast<void**>(&originalSetter));
    setterState.store(setterHook ? SetterHookLive : SetterHookInstallFail, std::memory_order_release);
    return setterHook != nullptr;
}

void refreshStatus() {
    if (setterHook) {
        statusText.store("capture: setter (primary)", std::memory_order_release);
        return;
    }
    // No setter hook: say WHY before falling back to the legacy wording —
    // "anchor mismatch" is the failure that must never be mistaken for a
    // plain miss on-device.
    switch (setterState.load(std::memory_order_acquire)) {
    case SetterHookUnresolved:
        statusText.store("capture: setter unresolved; legacy", std::memory_order_release);
        return;
    case SetterHookAnchorBad:
        statusText.store("capture: setter anchor mismatch; legacy", std::memory_order_release);
        return;
    case SetterHookInstallFail:
        statusText.store("capture: setter install failed; legacy", std::memory_order_release);
        return;
    default:
        break;
    }
    if (packetHook && levelInitHook) {
        statusText.store("capture: packet+object", std::memory_order_release);
    } else if (packetHook) {
        statusText.store("capture: packet (object pending ST-009)", std::memory_order_release);
    } else if (levelInitHook) {
        statusText.store("capture: object (packet unresolved)", std::memory_order_release);
    } else {
        statusText.store("capture: unresolved", std::memory_order_release);
    }
}

}

bool SeedCapture::install() {
    std::lock_guard lock(installMutex);
    using seedtaker::memory::SignatureId;
    if (!packetHook) {
        const auto address = seedtaker::memory::resolve(SignatureId::StartGameHandle);
        if (address) {
            packetHook = seedtaker::hooks::install(reinterpret_cast<void*>(address),
                                                   reinterpret_cast<void*>(startGameHandleDetour),
                                                   reinterpret_cast<void**>(&originalHandle));
        }
    }
    if (!levelInitHook) {
        const auto address = seedtaker::memory::resolve(SignatureId::LevelInit);
        if (address) {
            levelInitHook = seedtaker::hooks::install(reinterpret_cast<void*>(address),
                                                      reinterpret_cast<void*>(levelInitializeDetour),
                                                      reinterpret_cast<void**>(&originalLevelInit));
        }
    }
    // Primary source (F-088). VA-resolved with an anchor gate, so this is
    // attempted regardless of the signature-based hooks above.
    if (!setterHook) {
        installSetterHook();
    }
    refreshStatus();
    return setterHook != nullptr || packetHook != nullptr || levelInitHook != nullptr;
}

void SeedCapture::uninstall() {
    std::lock_guard lock(installMutex);
    if (packetHook) {
        seedtaker::hooks::remove(packetHook);
        packetHook = nullptr;
    }
    if (levelInitHook) {
        seedtaker::hooks::remove(levelInitHook);
        levelInitHook = nullptr;
    }
    if (setterHook) {
        seedtaker::hooks::remove(setterHook);
        setterHook = nullptr;
    }
    originalSetter = nullptr;
    statusText.store("capture: idle", std::memory_order_release);
}

bool SeedCapture::packetHookActive() {
    return packetHook != nullptr;
}

bool SeedCapture::levelInitHookActive() {
    return levelInitHook != nullptr;
}

bool SeedCapture::setterHookActive() {
    return setterHook != nullptr;
}

CaptureReading SeedCapture::packetReading() {
    const auto packed = packetPacked.load(std::memory_order_acquire);
    return CaptureReading{packed >> 32, static_cast<std::int32_t>(packed & 0xFFFFFFFFu)};
}

CaptureReading SeedCapture::objectReading() {
    const auto packed = objectPacked.load(std::memory_order_acquire);
    return CaptureReading{packed >> 32, static_cast<std::int32_t>(packed & 0xFFFFFFFFu)};
}

// Seqlock read (F-088): retry while a write is in flight. Bounded retries
// so a pathological writer can never wedge the frame thread; the setter
// fires only on world load, so one pass is the normal case.
SetterReading SeedCapture::setterReading() {
    SetterReading reading;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const auto begin = setterSeq.load(std::memory_order_acquire);
        if (begin & 1u) continue; // write in flight
        const auto value = setterValue.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        const auto end = setterSeq.load(std::memory_order_relaxed);
        if (begin == end) {
            reading.sequence = begin;
            reading.seed = value;
            return reading;
        }
    }
    return reading; // sequence 0 = no settled reading; caller keeps state.
}

std::uint64_t SeedCapture::rejectedPackets() {
    return rejectedCount.load(std::memory_order_relaxed);
}

std::uint64_t SeedCapture::typeMismatches() {
    return mismatchCount.load(std::memory_order_relaxed);
}

const char* SeedCapture::status() {
    return statusText.load(std::memory_order_acquire);
}

}
