#include <Geode/Geode.hpp>
#include <minimum.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#include <mmsystem.hpp>
#pragma comment(lib, "winmm.lib")
#endif

using namespace geode::prelude;

// Windows frame-pacing tweaks.
//
//   * 1 ms timer resolution (timeBeginPeriod): without this Windows may wake the
//     process only every 15.6 ms, which makes frame times jump between ~0 and ~16 ms.
//   * Opt out of power throttling (EcoQoS): keeps the game off the efficiency cores
//     and off the low-clock power plan Windows applies to background processes.
//   * Optional process priority (Above Normal / High).
//
// All three are only applied when the setting is on and the current state differs
// from the wanted state, so refreshConfig can call this every half second without
// cost. No-op on non-Windows.

namespace minimum {

#ifdef GEODE_IS_WINDOWS

    namespace {
        bool g_timerActive = false;
        bool g_powerOptOut = false;
        ProcessPriority g_priority = ProcessPriority::Normal;

        DWORD priorityClass(ProcessPriority p) {
            switch (p) {
                case ProcessPriority::AboveNormal: return ABOVE_NORMAL_PRIORITY_CLASS;
                case ProcessPriority::High:        return HIGH_PRIORITY_CLASS;
                default:                           return NORMAL_PRIORITY_CLASS;
            }
        }
    }

    void applySystemTuning() {
        auto const& cfg = config();

        // Timer resolution
        if (cfg.enabled && cfg.timerResolution) {
            if (!g_timerActive) {
                if (timeBeginPeriod(1) == TIMERR_NOERROR) {
                    g_timerActive = true;
                }
            }
        } else if (g_timerActive) {
            timeEndPeriod(1);
            g_timerActive = false;
        }

        // Power throttling opt-out
        if (cfg.enabled && cfg.disablePowerThrottling) {
            if (!g_powerOptOut) {
                PROCESS_POWER_THROTTLING_STATE state{};
                state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
                state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
                state.StateMask = 0; // clear = opt out
                if (SetProcessInformation(
                        GetCurrentProcess(),
                        ProcessPowerThrottling,
                        &state,
                        sizeof(state))) {
                    g_powerOptOut = true;
                }
            }
        } else if (g_powerOptOut) {
            PROCESS_POWER_THROTTLING_STATE state{};
            state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
            state.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED; // re-enable
            SetProcessInformation(
                GetCurrentProcess(),
                ProcessPowerThrottling,
                &state,
                sizeof(state));
            g_powerOptOut = false;
        }

        // Process priority
        ProcessPriority wanted = cfg.enabled ? cfg.priority : ProcessPriority::Normal;
        if (wanted != g_priority) {
            if (SetPriorityClass(GetCurrentProcess(), priorityClass(wanted))) {
                g_priority = wanted;
            }
        }
    }

#else

    void applySystemTuning() {}

#endif

}
