#include <Geode/Geode.hpp>
#include <minimum.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

using namespace geode::prelude;

// System tuning (Windows)
//
// Frame pacing problems that are not the game's fault usually come from the OS:
//
//   * Timer resolution. By default Windows wakes sleeping threads every ~15.6 ms.
//     When the game waits for the next frame it can oversleep by that much, which
//     shows up as uneven frame times. timeBeginPeriod(1) asks for 1 ms wakeups
//     (per process on Windows 10 2004+ and 11).
//   * Power throttling (EcoQoS). Windows can silently put a process on efficiency
//     cores / low clocks. We opt the game process out of that.
//   * Process priority. Optional. Above Normal is safe, High can starve OBS or
//     Discord, so it is off by default. Realtime is deliberately not offered.
//
// Every call is undone when the setting is switched off.

namespace minimum {

#ifdef GEODE_IS_WINDOWS

    namespace {

        // Same layout as PROCESS_POWER_THROTTLING_STATE. Declared here so the mod does
        // not depend on the Windows SDK version Geode happens to build with.
        struct PowerThrottlingState {
            ULONG version;
            ULONG controlMask;
            ULONG stateMask;
        };

        constexpr ULONG kPowerThrottlingVersion = 1;
        constexpr ULONG kPowerThrottlingExecutionSpeed = 0x1;
        constexpr int kProcessPowerThrottling = 4; // PROCESS_INFORMATION_CLASS::ProcessPowerThrottling

        using SetProcessInformationFn = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);

        bool g_timerApplied = false;
        bool g_throttleOptOutApplied = false;
        ProcessPriority g_priorityApplied = ProcessPriority::Normal;

        void setPowerThrottlingOptOut(bool optOut) {
            HMODULE kernel = GetModuleHandleA("kernel32.dll");
            if (!kernel) return;
            auto fn = reinterpret_cast<SetProcessInformationFn>(
                reinterpret_cast<void*>(GetProcAddress(kernel, "SetProcessInformation"))
            );
            if (!fn) return; // Windows older than 8, nothing to do.

            PowerThrottlingState state{};
            state.version = kPowerThrottlingVersion;
            if (optOut) {
                // control = we decide, state = 0 -> throttling off.
                state.controlMask = kPowerThrottlingExecutionSpeed;
                state.stateMask = 0;
            }
            else {
                // control = 0 -> back to system managed.
                state.controlMask = 0;
                state.stateMask = 0;
            }
            fn(GetCurrentProcess(), kProcessPowerThrottling, &state, sizeof(state));
        }

        DWORD priorityClassFor(ProcessPriority p) {
            switch (p) {
                case ProcessPriority::AboveNormal: return ABOVE_NORMAL_PRIORITY_CLASS;
                case ProcessPriority::High: return HIGH_PRIORITY_CLASS;
                default: return NORMAL_PRIORITY_CLASS;
            }
        }

    }

    void applySystemTuning() {
        auto const& cfg = config();

        bool wantTimer = cfg.enabled && cfg.timerResolution;
        if (wantTimer != g_timerApplied) {
            if (wantTimer) {
                if (timeBeginPeriod(1) == TIMERR_NOERROR) g_timerApplied = true;
            }
            else {
                timeEndPeriod(1);
                g_timerApplied = false;
            }
        }

        bool wantOptOut = cfg.enabled && cfg.disablePowerThrottling;
        if (wantOptOut != g_throttleOptOutApplied) {
            setPowerThrottlingOptOut(wantOptOut);
            g_throttleOptOutApplied = wantOptOut;
        }

        ProcessPriority wantPriority = cfg.enabled ? cfg.priority : ProcessPriority::Normal;
        if (wantPriority != g_priorityApplied) {
            if (SetPriorityClass(GetCurrentProcess(), priorityClassFor(wantPriority))) {
                g_priorityApplied = wantPriority;
            }
        }
    }

#else

    void applySystemTuning() {}

#endif

}
