#include <Geode/Geode.hpp>
#include <minimum.hpp>
#include <cstdint>
#include <deque>

#if defined(GEODE_IS_WINDOWS)
#include <Windows.h>
#elif defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS) || defined(GEODE_IS_ANDROID)
#include <dlfcn.h>
#endif

using namespace geode::prelude;

// Low latency mode (all platforms).
//
// Input to screen latency is mostly queueing. The GPU driver may keep one to three finished
// frames in flight, so a click can show up one to three frames late.
//
// The old version of this mode called glFinish() after every frame. That makes the CPU wait
// until the GPU is completely idle, so the CPU and the GPU never work at the same time. On a
// GPU bound level that cost real FPS, which is why the frame gate had to switch it off.
//
// This version only limits the queue. Before a frame is drawn, the CPU waits until the frame
// submitted kMaxFramesInFlight frames ago has finished on the GPU (a GL fence). While the GPU
// is busy there is always a frame waiting behind the one it is drawing, so it does not go idle,
// but the driver never holds more than kMaxFramesInFlight frames. The fence is only placed and
// waited on while the frame gate allows the mode (see LatencyGate), so nothing runs otherwise.
//
// Fence functions need GL 3.2 or GLES 3.0. Where they cannot be found the mode reports
// "sync n/a" and does nothing. Nothing here touches physics or timing.
//
// Fence functions are looked up at runtime (the result is logged once), so the mod has no link
// dependency on a GL library and the same file builds on every platform.

namespace minimum {

    namespace {
        using GlSync = void*;
        using FenceSyncFn = GlSync (*)(unsigned int condition, unsigned int flags);
        using ClientWaitSyncFn = unsigned int (*)(GlSync sync, unsigned int flags, uint64_t timeoutNs);
        using DeleteSyncFn = void (*)(GlSync sync);

        // Values from the GL 3.2 and GLES 3.0 specifications.
        constexpr unsigned int kSyncGpuCommandsComplete = 0x9117;
        constexpr unsigned int kSyncFlushCommandsBit = 0x00000001;
        constexpr unsigned int kAlreadySignaled = 0x911A;
        constexpr unsigned int kConditionSatisfied = 0x911C;
        constexpr unsigned int kWaitFailed = 0x911D;

        // How many frames may be queued in the driver at once.
        constexpr size_t kMaxFramesInFlight = 2;
        // One wait step is 2 ms. After 50 steps (100 ms) the fence is dropped, so a stuck driver
        // can not freeze the game.
        constexpr uint64_t kWaitStepNs = 2000000;
        constexpr int kMaxWaitSteps = 50;

        struct Fns {
            FenceSyncFn fence = nullptr;
            ClientWaitSyncFn wait = nullptr;
            DeleteSyncFn del = nullptr;
            char const* source = "none";

            bool complete() const { return fence != nullptr && wait != nullptr && del != nullptr; }
        };

#if defined(GEODE_IS_WINDOWS)
        using WglGetProcAddressFn = PROC (WINAPI*)(LPCSTR);

        // Core GL 3.2 functions are often not exported by opengl32.dll, so the driver is asked first
        // through wglGetProcAddress. That needs a current context, which is true inside drawScene.
        void* glProc(char const* name) {
            HMODULE const gl = GetModuleHandleA("opengl32.dll");
            if (!gl) return nullptr;

            if (auto* getter = reinterpret_cast<WglGetProcAddressFn>(
                    reinterpret_cast<void*>(GetProcAddress(gl, "wglGetProcAddress")))) {
                void* sym = reinterpret_cast<void*>(getter(name));
                // wglGetProcAddress reports failure with 0, 1, 2, 3 or -1.
                auto const raw = reinterpret_cast<intptr_t>(sym);
                if (sym && raw != 1 && raw != 2 && raw != 3 && raw != -1) return sym;
            }
            return reinterpret_cast<void*>(GetProcAddress(gl, name));
        }
#else
        void* glProc(char const* name) {
            if (void* sym = dlsym(RTLD_DEFAULT, name)) return sym;
#if defined(GEODE_IS_ANDROID)
            // The usual GL libraries on Android. They are already loaded by the game, so dlopen
            // only hands out the existing handle.
            for (char const* lib : {"libGLESv3.so", "libGLESv2.so"}) {
                if (void* handle = dlopen(lib, RTLD_NOW)) {
                    if (void* sym = dlsym(handle, name)) return sym;
                }
            }
#endif
            return nullptr;
        }
#endif

        Fns const& fns() {
            static Fns const resolved = [] {
                Fns f;
                f.fence = reinterpret_cast<FenceSyncFn>(glProc("glFenceSync"));
                f.wait = reinterpret_cast<ClientWaitSyncFn>(glProc("glClientWaitSync"));
                f.del = reinterpret_cast<DeleteSyncFn>(glProc("glDeleteSync"));
                if (f.complete()) {
                    f.source = "driver";
                    log::info("Minimum: low latency fences ready (GL sync objects found)");
                }
                else {
                    log::warn("Minimum: GL sync objects were not found on this device, Low Latency Mode can not work here");
                }
                return f;
            }();
            return resolved;
        }

        // Fences of the frames that were submitted and not waited on yet, oldest first.
        std::deque<GlSync> g_inFlight;
    }

    bool hardGpuSyncAvailable() {
        return fns().complete();
    }

    // Before a frame is drawn: while the queue is full, wait for the oldest fence.
    void frameQueueWait() {
        auto const& f = fns();
        if (!f.complete()) return;

        while (g_inFlight.size() >= kMaxFramesInFlight) {
            GlSync const oldest = g_inFlight.front();
            g_inFlight.pop_front();

            // The first step flushes the commands, so the fence can actually be reached.
            for (int step = 0; step < kMaxWaitSteps; ++step) {
                unsigned int const flags = step == 0 ? kSyncFlushCommandsBit : 0u;
                unsigned int const result = f.wait(oldest, flags, kWaitStepNs);
                if (result == kAlreadySignaled || result == kConditionSatisfied || result == kWaitFailed) break;
            }
            f.del(oldest);
        }
    }

    // After a frame was drawn: place a fence behind it.
    void frameQueueSubmit() {
        auto const& f = fns();
        if (!f.complete()) return;

        if (GlSync const fence = f.fence(kSyncGpuCommandsComplete, 0u)) {
            g_inFlight.push_back(fence);
        }
    }

    // Called when the mode is not running: drops every fence still in the queue.
    void frameQueueReset() {
        auto const& f = fns();
        while (!g_inFlight.empty()) {
            if (f.del) f.del(g_inFlight.front());
            g_inFlight.pop_front();
        }
    }

}
