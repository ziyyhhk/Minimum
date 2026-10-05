#include <Geode/Geode.hpp>
#include <cstddef>
#include <cstdint>
#include <minimum.hpp>

#ifdef GEODE_IS_ANDROID
#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

using namespace geode::prelude;

// Android CPU hint.
//
// Phones keep the CPU slow to save battery and speed it up when they see load. A game frame
// that is suddenly heavier than the one before arrives while the CPU is still ramping up,
// which is one common source of stutter on phones.
//
// Android 12+ has an API for this (ADPF, "performance hints"): the game tells the system how
// long a frame should take and how long the last one really took on the CPU, and the system
// picks the CPU speed ahead of time. Android 13 exposes it to native code as
// APerformanceHint_*. The functions are looked up at runtime, so the mod still loads (and
// simply does nothing) on older versions and on devices that do not provide the service.
//
// It changes how fast the CPU runs, nothing else. No game logic, timing or physics is touched.

namespace minimum {

#ifdef GEODE_IS_ANDROID

    namespace {

        using GetManagerFn = void* (*)();
        using CreateSessionFn = void* (*)(void*, int32_t const*, size_t, int64_t);
        using UpdateTargetFn = int (*)(void*, int64_t);
        using ReportFn = int (*)(void*, int64_t);
        using CloseFn = void (*)(void*);

        struct HintApi {
            bool resolved = false;
            bool usable = false;
            void* manager = nullptr;
            CreateSessionFn createSession = nullptr;
            UpdateTargetFn updateTarget = nullptr;
            ReportFn report = nullptr;
            CloseFn closeSession = nullptr;
        };

        HintApi g_api;
        void* g_session = nullptr;
        int g_sessionThread = 0;
        int64_t g_sessionTargetNs = 0;
        int g_failures = 0;

        template <class Fn>
        Fn lookup(void* library, char const* name) {
            return reinterpret_cast<Fn>(dlsym(library, name));
        }

        void resolveApi() {
            g_api.resolved = true;

            void* library = dlopen("libandroid.so", RTLD_NOW);
            if (!library) return;

            auto getManager = lookup<GetManagerFn>(library, "APerformanceHint_getManager");
            g_api.createSession = lookup<CreateSessionFn>(library, "APerformanceHint_createSession");
            g_api.updateTarget = lookup<UpdateTargetFn>(library, "APerformanceHint_updateTargetWorkDuration");
            g_api.report = lookup<ReportFn>(library, "APerformanceHint_reportActualWorkDuration");
            g_api.closeSession = lookup<CloseFn>(library, "APerformanceHint_closeSession");

            if (!getManager || !g_api.createSession || !g_api.updateTarget || !g_api.report || !g_api.closeSession) {
                log::info("Minimum: Android performance hints are not available on this version");
                return;
            }

            g_api.manager = getManager();
            g_api.usable = g_api.manager != nullptr;
            log::info(
                "Minimum: Android performance hints {}",
                g_api.usable ? "are available" : "are not provided by this device"
            );
        }

        void dropSession() {
            if (g_session && g_api.closeSession) g_api.closeSession(g_session);
            g_session = nullptr;
            g_sessionThread = 0;
            g_sessionTargetNs = 0;
        }

    }

    void reportCpuWork(int64_t workNs) {
        auto const& cfg = config();

        // After a few failed calls give up for the rest of the session instead of retrying
        // (and failing) every frame.
        bool const want = cfg.enabled && cfg.androidCpuHint && g_failures < 5;
        if (!want) {
            if (g_session) dropSession();
            return;
        }
        if (workNs <= 0) return;

        if (!g_api.resolved) resolveApi();
        if (!g_api.usable) return;

        // The session belongs to the thread that does the frame work: the render thread,
        // which is the one calling this from the drawScene hook.
        int const thread = static_cast<int>(syscall(SYS_gettid));
        int64_t const targetNs = static_cast<int64_t>(1e9 / cfg.targetFps);

        if (g_session && thread != g_sessionThread) dropSession();

        if (!g_session) {
            int32_t const threads[1] = {static_cast<int32_t>(thread)};
            g_session = g_api.createSession(g_api.manager, threads, 1, targetNs);
            if (!g_session) {
                ++g_failures;
                return;
            }
            g_sessionThread = thread;
            g_sessionTargetNs = targetNs;
        }
        else if (targetNs != g_sessionTargetNs) {
            if (g_api.updateTarget(g_session, targetNs) == 0) g_sessionTargetNs = targetNs;
            else ++g_failures;
        }

        if (g_api.report(g_session, workNs) != 0) ++g_failures;
    }

#else

    void reportCpuWork(int64_t) {}

#endif

}
