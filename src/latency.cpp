#include <Geode/Geode.hpp>
#include <minimum.hpp>

#if defined(GEODE_IS_WINDOWS)
#include <Windows.h>
#elif defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS) || defined(GEODE_IS_ANDROID)
#include <dlfcn.h>
#endif

using namespace geode::prelude;

// Low latency mode (all platforms).
//
// Input-to-screen latency is mostly queueing: after the game submits a frame, the GPU
// driver is allowed to keep one to three finished frames in flight, so a click is shown
// one to three frames late. Calling glFinish() once the frame has been submitted makes the
// CPU wait until the GPU has caught up, which empties that queue. It is the same
// "hard GPU sync" option emulators such as RetroArch offer for the same reason.
//
// The cost is that CPU and GPU no longer overlap, so it is only applied while the game
// is holding its frame rate (see the frame gate), and it switches itself off for a while
// when it costs frames. It never touches physics or timing.
//
// Phones: the mode runs there too. On Android the buffer swap is done by the system after
// drawScene returns, so the sync happens right before the swap: the frame is completely
// drawn when it is handed over. On a phone whose bottleneck is the CPU it changes nothing,
// which is why the popup and the detailed FPS counter show whether it is active.
//
// glFinish is looked up at runtime (and the result is logged once), so the mod has no link
// dependency on the GL library and the same file builds on every platform. Several
// libraries are tried because the name differs between devices and drivers.

namespace minimum {

    namespace {
        using GlFinishFn = void (*)();

        struct Resolved {
            GlFinishFn fn = nullptr;
            char const* source = "none";
        };

        Resolved resolveGlFinish() {
            Resolved r;

#if defined(GEODE_IS_WINDOWS)
            for (char const* name : {"opengl32.dll", "libGLESv2.dll"}) {
                if (HMODULE gl = GetModuleHandleA(name)) {
                    if (auto* sym = GetProcAddress(gl, "glFinish")) {
                        r.fn = reinterpret_cast<GlFinishFn>(reinterpret_cast<void*>(sym));
                        r.source = name;
                        return r;
                    }
                }
            }
#elif defined(GEODE_IS_ANDROID) || defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
            // 1) Already visible to the whole process (iOS / macOS link the GL framework, many
            //    Android builds expose libGLESv2 globally).
            if (void* sym = dlsym(RTLD_DEFAULT, "glFinish")) {
                r.fn = reinterpret_cast<GlFinishFn>(sym);
                r.source = "process";
                return r;
            }
#if defined(GEODE_IS_ANDROID)
            // 2) The usual Android GL libraries. They are public NDK libraries and already
            //    loaded by the game, dlopen only hands out the existing handle.
            for (char const* name : {"libGLESv2.so", "libGLESv3.so", "libGLESv1_CM.so"}) {
                if (void* lib = dlopen(name, RTLD_NOW)) {
                    if (void* sym = dlsym(lib, "glFinish")) {
                        r.fn = reinterpret_cast<GlFinishFn>(sym);
                        r.source = name;
                        return r;
                    }
                }
            }
#endif
#endif
            return r;
        }

        Resolved const& resolved() {
            static Resolved const r = [] {
                Resolved found = resolveGlFinish();
                if (found.fn) {
                    log::info("Minimum: low latency sync ready (glFinish from {})", found.source);
                }
                else {
                    log::warn("Minimum: glFinish was not found on this device, Low Latency Mode can not work here");
                }
                return found;
            }();
            return r;
        }
    }

    bool hardGpuSyncAvailable() {
        return resolved().fn != nullptr;
    }

    void hardGpuSync() {
        if (auto fn = resolved().fn) fn();
    }

}
