#include "SeedCapture.hpp"
#include "VtableScan.hpp"

#include "core/memory/Hooks.hpp"
#include <seedtaker/memory/Signatures.hpp>
#include <seedtaker/sdk/Memory.hpp>
#include <seedtaker/sdk/offsets/Seed.hpp>
#include <atomic>
#include <cstdint>
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

void refreshStatus() {
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
    refreshStatus();
    return packetHook != nullptr || levelInitHook != nullptr;
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
    statusText.store("capture: idle", std::memory_order_release);
}

bool SeedCapture::packetHookActive() {
    return packetHook != nullptr;
}

bool SeedCapture::levelInitHookActive() {
    return levelInitHook != nullptr;
}

CaptureReading SeedCapture::packetReading() {
    const auto packed = packetPacked.load(std::memory_order_acquire);
    return CaptureReading{packed >> 32, static_cast<std::int32_t>(packed & 0xFFFFFFFFu)};
}

CaptureReading SeedCapture::objectReading() {
    const auto packed = objectPacked.load(std::memory_order_acquire);
    return CaptureReading{packed >> 32, static_cast<std::int32_t>(packed & 0xFFFFFFFFu)};
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
