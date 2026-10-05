#include <Geode/Geode.hpp>
#include <minimum.hpp>

#if defined(GEODE_IS_WINDOWS)
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

// Low latency mode (Windows, macOS, Android, iOS).
//
// Input-to-screen latency is mostly queueing: after the game submits a frame,
// the GPU driver is allowed to keep one to three finished frames in flight, so
// a click shows up on screen one to three frames late. Calling glFinish()
// right after the buffer swap makes the CPU wait until the GPU has caught up,
// which empties that queue. Same "hard GPU sync" trick emulators like
// RetroArch use, for the same reason.
//
// The cost is that CPU and GPU no longer overlap, so the frame gate only
// allows it while the game is comfortably holding its frame rate, and backs
// off for a while if it starts costing frames.
//
// glFinish is looked up at runtime so the mod has no link dependency on any
// GL library and the same file builds on every platform. Desktop GL and
// GLES both export the symbol under the plain name "glFinish".

namespace minimum {

    namespace {
        using GlFinishFn = void (*)();

        GlFinishFn resolveGlFinish() {
#if defined(GEODE_IS_WINDOWS)
            HMODULE gl = GetModuleHandleA("opengl32.dll");
            if (!gl) return nullptr;
            return reinterpret_cast<GlFinishFn>(reinterpret_cast<void*>(
                GetProcAddress(gl, "glFinish")
            ));
#elif defined(GEODE_IS_ANDROID)
            // The game already links GLES, so the symbol is usually right
            // there. Fall back to opening the library explicitly if not.
            if (void* sym = dlsym(RTLD_DEFAULT, "glFinish")) {
                return reinterpret_cast<GlFinishFn>(sym);
            }
            static void* gles = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
            if (!gles) return nullptr;
            return reinterpret_cast<GlFinishFn>(dlsym(gles, "glFinish"));
#else
            // macOS and iOS: the process already links the GL framework.
            return reinterpret_cast<GlFinishFn>(dlsym(RTLD_DEFAULT, "glFinish"));
#endif
        }
    }

    void hardGpuSync() {
        static GlFinishFn const fn = resolveGlFinish();
        if (fn) fn();
    }

}
