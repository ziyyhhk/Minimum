#include <Geode/Geode.hpp>
#include <minimum.hpp>

#if defined(GEODE_IS_WINDOWS)
#include <Windows.h>
#elif defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS) || defined(GEODE_IS_ANDROID)
#include <dlfcn.h>
#endif

// Low latency mode (all platforms).
//
// Input-to-screen latency is mostly queueing: after the game submits a frame, the GPU
// driver is allowed to keep one to three finished frames in flight, so a click is shown
// one to three frames late. Calling glFinish() after the frame is submitted makes the
// CPU wait until the GPU has caught up, which empties that queue. It is the same
// "hard GPU sync" option emulators such as RetroArch offer for the same reason.
//
// The cost is that CPU and GPU no longer overlap, so it is only applied while the game
// is holding its frame rate (see the frame gate), and it switches itself off for a while
// when it costs frames. It never touches physics or timing.
//
// How much it helps depends on the device: it only does something when the GPU is the
// one running behind. On a phone that is CPU bound it changes nothing.
//
// glFinish is looked up at runtime so the mod has no link dependency on the GL library
// and the same file builds on every platform.

namespace minimum {

    namespace {
        using GlFinishFn = void (*)();

        GlFinishFn resolveGlFinish() {
#if defined(GEODE_IS_WINDOWS)
            HMODULE gl = GetModuleHandleA("opengl32.dll");
            if (!gl) return nullptr;
            return reinterpret_cast<GlFinishFn>(reinterpret_cast<void*>(GetProcAddress(gl, "glFinish")));
#elif defined(GEODE_IS_ANDROID)
            // The game draws with OpenGL ES 2, which lives in libGLESv2.so. It is already
            // loaded by the game, dlopen only hands out the existing handle.
            void* gl = dlopen("libGLESv2.so", RTLD_NOW);
            if (!gl) return nullptr;
            return reinterpret_cast<GlFinishFn>(dlsym(gl, "glFinish"));
#elif defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
            return reinterpret_cast<GlFinishFn>(dlsym(RTLD_DEFAULT, "glFinish"));
#else
            return nullptr;
#endif
        }
    }

    void hardGpuSync() {
        static GlFinishFn const fn = resolveGlFinish();
        if (fn) fn();
    }

}
