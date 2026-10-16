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

// Android CPU hint and thermal status.
//
// Phones keep the CPU slow to save battery and speed it up when they see load. A game frame
// that is suddenly heavier than the one before arrives while the CPU is still ramping up,
// which is one common source of stutter on phones.
//
// Android has an API for this (ADPF, "performance hints"): the game tells the system how
// long a frame should take and how long the last one really took on the CPU, and the system
// picks the CPU speed ahead of time. The functions are looked up at runtime, so the mod still
// loads (and simply does nothing) on older versions and on devices that do not provide the service.
//
// UNVERIFIED: the minimum Android version of the APerformanceHint_* functions is the API level
// in __INTRODUCED_IN in android/performance_hint.h. That header could not be read in this session
// (the NDK reference page has no availability line), so "Android 13" in the README and in mod.json
// is not confirmed. The runtime lookup below is what actually decides.
//
// Thermal status (Android): AThermal_getCurrentThermalStatus, API 30 according to the NDK
// reference (AThermal_* group). It is polled about once a second and feeds the thermal part of
// the particle cap (see ThermalGovernor). It is polled and not read from a listener, because
// listener callbacks run on a system thread.
//
// Neither changes game logic, timing or physics. They change the CPU speed and the particle cap.

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

        // ---- Thermal status ----------------------------------------------------------------
        // UNVERIFIED: the C signatures below (AThermalManager* AThermal_acquireManager(void),
        // int32_t AThermal_getCurrentThermalStatus(AThermalManager*), void AThermal_releaseManager
        // (AThermalManager*)). The names, API level 30 and the status values were checked against
        // the NDK reference, not the header.
        using AcquireThermalFn = void* (*)();
        using ThermalStatusFn = int32_t (*)(void*);

        struct ThermalApi {
            bool resolved = false;
            void* manager = nullptr;
            ThermalStatusFn status = nullptr;
        };

        ThermalApi g_thermal;
        double g_lastThermalPoll = -1.0;

        void resolveThermal() {
            g_thermal.resolved = true;

            void* library = dlopen("libandroid.so", RTLD_NOW);
            if (!library) return;

            auto acquire = lookup<AcquireThermalFn>(library, "AThermal_acquireManager");
            g_thermal.status = lookup<ThermalStatusFn>(library, "AThermal_getCurrentThermalStatus");
            if (!acquire || !g_thermal.status) {
                log::info("Minimum: thermal status is not available on this version");
                g_thermal.status = nullptr;
                return;
            }
            g_thermal.manager = acquire();
            if (!g_thermal.manager) {
                g_thermal.status = nullptr;
                log::info("Minimum: this device does not provide a thermal manager");
            }
        }

        // The status (AThermalStatus, -1 on error or when unavailable).
        int readThermalStatus() {
            if (!g_thermal.resolved) resolveThermal();
            if (!g_thermal.manager || !g_thermal.status) return -1;
            return static_cast<int>(g_thermal.status(g_thermal.manager));
        }

    }

    void pollThermal(double nowSec) {
        if (g_lastThermalPoll >= 0.0 && nowSec - g_lastThermalPoll < 1.0) return;
        g_lastThermalPoll = nowSec;

        auto const& cfg = config();
        bool const wanted = cfg.enabled && cfg.capParticles && cfg.adaptiveCap;
        // When the response is off, -1 is passed on, which also clears any step it had taken.
        noteThermalStatus(wanted ? readThermalStatus() : -1, nowSec);
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

    void pollThermal(double) {}

    void reportCpuWork(int64_t) {}

#endif

}
