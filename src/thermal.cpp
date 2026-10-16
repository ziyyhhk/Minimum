#include <Geode/Geode.hpp>
#include <minimum.hpp>

#ifdef GEODE_IS_ANDROID
#include <dlfcn.h>
#endif

using namespace geode::prelude;

// Thermal status (Android 11+, API level 30).
//
// The NDK has AThermal_acquireManager / AThermal_releaseManager / AThermal_getCurrentThermalStatus
// in <android/thermal.h>, introduced in API 30. They are looked up at runtime, the same way
// perf_hint.cpp does it, so the mod still loads (and simply reports "unknown") on older versions
// and on devices that do not provide the service. Nothing from the NDK is linked.
//
// The status is a plain int (see ThermalGuard in minimum_logic.hpp): 0 none, 1 light, 2 moderate,
// 3 severe, 4 critical, 5 emergency, 6 shutdown, -1 error. It only feeds the particle cap.
//
// iOS: not implemented. The equivalent is NSProcessInfo.thermalState, which is Objective-C and
// cannot be called from this C++ file; it would need an .mm file and build changes that cannot
// be checked without the iOS toolchain.

namespace minimum {

#ifdef GEODE_IS_ANDROID

    namespace {

        using AcquireManagerFn = void* (*)();
        using GetStatusFn = int (*)(void*);

        struct ThermalApi {
            bool resolved = false;
            bool usable = false;
            void* manager = nullptr;
            GetStatusFn getStatus = nullptr;
        };

        ThermalApi g_api;

        void resolveApi() {
            g_api.resolved = true;

            // The header does not say which library exports these, so look in libandroid.so
            // first (where the other android/*.h functions live, see perf_hint.cpp), then in
            // everything that is already loaded.
            void* library = dlopen("libandroid.so", RTLD_NOW);

            auto find = [&](char const* name) -> void* {
                void* symbol = library ? dlsym(library, name) : nullptr;
                return symbol ? symbol : dlsym(RTLD_DEFAULT, name);
            };

            auto acquire = reinterpret_cast<AcquireManagerFn>(find("AThermal_acquireManager"));
            g_api.getStatus = reinterpret_cast<GetStatusFn>(find("AThermal_getCurrentThermalStatus"));

            if (!acquire || !g_api.getStatus) {
                log::info("Minimum: Android thermal status is not available on this version");
                return;
            }

            // Kept for the lifetime of the process, there is one per process anyway.
            g_api.manager = acquire();
            g_api.usable = g_api.manager != nullptr;
            log::info("Minimum: Android thermal status {}", g_api.usable ? "available" : "not available (no manager)");
        }

    }

    int readThermalStatus() {
        if (!g_api.resolved) resolveApi();
        if (!g_api.usable) return -1;
        int const status = g_api.getStatus(g_api.manager);
        return status < 0 ? -1 : status;
    }

#else

    int readThermalStatus() {
        return -1;
    }

#endif

}
