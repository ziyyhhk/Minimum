#include <Geode/Geode.hpp>
#include <minimum.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// Windows system tuning.
//
// Three small things, all applied only when the wanted state differs from the
// state we last applied (refreshConfig calls us twice a second, so this must
// be cheap):
//
//   * Timer resolution: Windows wakes sleeping threads every ~15.6 ms by
//     default. Asking for 1 ms wakeups makes the game's frame waits land on
//     time, which evens out frame pacing. Done through NtSetTimerResolution
//     (looked up at runtime) so there is no winmm link dependency.
//
//   * Power throttling (EcoQoS): Windows 11 is allowed to move background-ish
//     processes to efficiency cores and lower clocks. Opt the game out while
//     the setting is on.
//
//   * Process priority: plain SetPriorityClass. "Above Normal" is the safe
//     one; "High" is there for people who know they want it.

namespace minimum {

#ifdef GEODE_IS_WINDOWS

    namespace {
        // -1 = not applied yet, so the first call always applies.
        int g_timerApplied = -1;
        int g_throttleApplied = -1;
        int g_priorityApplied = -1;

        void setTimerResolution(bool enable) {
            int const wanted = enable ? 1 : 0;
            if (wanted == g_timerApplied) return;

            using NtSetTimerResolutionFn = LONG(NTAPI*)(ULONG, BOOLEAN, PULONG);
            static NtSetTimerResolutionFn const fn = [] {
                HMODULE ntdll = GetModuleHandleA("ntdll.dll");
                if (!ntdll) return NtSetTimerResolutionFn(nullptr);
                return reinterpret_cast<NtSetTimerResolutionFn>(reinterpret_cast<void*>(
                    GetProcAddress(ntdll, "NtSetTimerResolution")
                ));
            }();
            if (!fn) return;

            ULONG current = 0;
            // 10000 units of 100 ns = 1 ms.
            if (fn(10000, enable ? TRUE : FALSE, &current) >= 0) {
                g_timerApplied = wanted;
            }
        }

        void setPowerThrottling(bool disable) {
            int const wanted = disable ? 1 : 0;
            if (wanted == g_throttleApplied) return;

            PROCESS_POWER_THROTTLING_STATE state{};
            state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
            // StateMask 0 = throttling off, EXECUTION_SPEED = back to default.
            state.StateMask = disable ? 0 : PROCESS_POWER_THROTTLING_EXECUTION_SPEED;

            if (SetProcessInformation(
                GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state)
            )) {
                g_throttleApplied = wanted;
            }
        }

        void setPriority(ProcessPriority priority) {
            int const wanted = static_cast<int>(priority);
            if (wanted == g_priorityApplied) return;

            DWORD cls = NORMAL_PRIORITY_CLASS;
            if (priority == ProcessPriority::AboveNormal) cls = ABOVE_NORMAL_PRIORITY_CLASS;
            else if (priority == ProcessPriority::High) cls = HIGH_PRIORITY_CLASS;

            if (SetPriorityClass(GetCurrentProcess(), cls)) {
                g_priorityApplied = wanted;
            }
        }
    }

    void applySystemTuning() {
        auto const& cfg = config();
        if (!cfg.enabled) return;
        setTimerResolution(cfg.timerResolution);
        setPowerThrottling(cfg.disablePowerThrottling);
        setPriority(cfg.priority);
    }

#else

    void applySystemTuning() {}

#endif

}
