#include <Geode/Geode.hpp>
#include <minimum.hpp>

#if defined(GEODE_IS_WINDOWS)
#include <Windows.h>
#elif defined(GEODE_IS_MACOS)
#include <dlfcn.h>
#endif

// Low latency mode (Windows and macOS).
//
// Input-to-screen latency is mostly queueing: after the game submits a frame, the GPU
// driver is allowed to keep one to three finished frames in flight, so a click is shown
// one to three frames late. Calling glFinish() right after the buffer swap makes the
// CPU wait until the GPU has caught up, which empties that queue. It is the same
// "hard GPU sync" option emulators such as RetroArch offer for the same reason.
//
// The cost is that CPU and GPU no longer overlap, so it is only applied while the game
// is holding its frame rate (see the frame gate). It never touches physics or timing.
//
// glFinish is looked up at runtime so the mod has no link dependency on the GL library
// and the same file builds on every platform.

namespace minimum {

#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_MACOS)

    namespace {
        using GlFinishFn = void (*)();

        GlFinishFn resolveGlFinish() {
#if defined(GEODE_IS_WINDOWS)
            HMODULE gl = GetModuleHandleA("opengl32.dll");
            if (!gl) return nullptr;
            return reinterpret_cast<GlFinishFn>(reinterpret_cast<void*>(GetProcAddress(gl, "glFinish")));
#else
            return reinterpret_cast<GlFinishFn>(dlsym(RTLD_DEFAULT, "glFinish"));
#endif
        }
    }

    void hardGpuSync() {
        static GlFinishFn const fn = resolveGlFinish();
        if (fn) fn();
    }

#else

    void hardGpuSync() {}

#endif

}
