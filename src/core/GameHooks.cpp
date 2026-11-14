#include "GameHooks.hpp"
#include <seedtaker/Version.hpp>

#include "core/memory/Hooks.hpp"
#include <seedtaker/events/EventBus.hpp>
#include <seedtaker/events/FrameEvent.hpp>
#include <seedtaker/memory/Signatures.hpp>
#include <EGL/egl.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace seedtaker::core::gamehooks {
namespace {
using namespace seedtaker::events;
using seedtaker::memory::SignatureId;

using EglSwapBuffersFn = EGLBoolean(*)(EGLDisplay, EGLSurface);

EglSwapBuffersFn swapBuffersOriginal = nullptr;
// Always null until a ClientInstance capture hook lands (pending ST-001).
std::atomic<void*> currentClientInstance = nullptr;
std::array<seedtaker::hooks::Handle, 8> handles{};
std::size_t handleCount = 0;
std::mutex installMutex;
bool installed = false;

template <class Function>
bool hookSignature(SignatureId id, void* detour, Function** original) {
    const auto address = seedtaker::memory::resolve(id);
    if (!address) return false;
    seedtaker::hooks::Handle handle = seedtaker::hooks::install(reinterpret_cast<void*>(address), detour, reinterpret_cast<void**>(original));
    if (!handle) return false;
    if (handleCount < handles.size()) handles[handleCount++] = handle;
    return true;
}

EGLBoolean swapBuffersDetour(EGLDisplay display, EGLSurface surface) {
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        FrameEvent event;
        bus().publish(event);
    }
    return swapBuffersOriginal ? swapBuffersOriginal(display, surface) : EGL_FALSE;
}

bool hookEgl() {
    auto egl = seedtaker::hooks::openLibrary("libEGL.so");
    if (!egl) return false;
    const auto address = seedtaker::hooks::symbol(egl, "eglSwapBuffers");
    if (!address) {
        seedtaker::hooks::closeLibrary(egl);
        return false;
    }
    auto handle = seedtaker::hooks::install(reinterpret_cast<void*>(address), reinterpret_cast<void*>(swapBuffersDetour), reinterpret_cast<void**>(&swapBuffersOriginal));
    seedtaker::hooks::closeLibrary(egl);
    if (!handle) return false;
    if (handleCount < handles.size()) handles[handleCount++] = handle;
    return true;
}
}

// The EGL frame pump is a system-API hook (dlsym "eglSwapBuffers") and needs
// no game-binary knowledge. Game-function hooks are module-owned (see
// SeedModule -> capture::SeedCapture, BedrockTools module-hooks pattern),
// not installed here. Reference for the overall shape:
// BedrockTools/src/core/GameHooks.cpp.
bool install() {
    std::lock_guard lock(installMutex);
    if (installed) return true;
    hookEgl();
    installed = swapBuffersOriginal != nullptr;
    return installed;
}

void uninstall() {
    std::lock_guard lock(installMutex);
    for (std::size_t i = 0; i < handleCount; ++i) {
        if (handles[i]) seedtaker::hooks::remove(handles[i]);
        handles[i] = nullptr;
    }
    handleCount = 0;
    installed = false;
    currentClientInstance.store(nullptr, std::memory_order_release);
}

void* clientInstance() {
    return currentClientInstance.load(std::memory_order_acquire);
}

}
