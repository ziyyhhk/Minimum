#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <chrono>
#include <minimum.hpp>
#include <minimum_logic.hpp>
#include <thread>

using namespace geode::prelude;

// Frame rate (all platforms).
//
// The game asks the system for a frame interval through CCDirector::setAnimationInterval
// (1/60 s on a phone by default). This hook records what the game asked for and passes on the
// interval the settings want:
//
//   * Android and iOS, "Unlock Frame Rate" on: an interval slower than the limit is shortened to
//     the limit. A faster request is kept. The screen's refresh rate and vsync still decide the
//     real maximum. Off: the game's request goes through unchanged, as before.
//   * Windows and macOS: nothing changes here. The recorded request tells the precise frame
//     limiter (see preciseFrameWait) whether the game's own cap is off.
//
// The hook is installed on every platform, because the desktop limiter needs the recorded request.
// Its body does nothing but record and pass the value on unless the unlock is on, so it costs
// one comparison per call.
//
// Only the frame interval changes. Game timing and physics are not touched.

namespace {

    // The interval (seconds per frame) the game asked for last. 0 means "no limit". -1 means the
    // game has not asked for anything yet.
    double g_requested = -1.0;
    // The interval that was passed on to the director last.
    double g_passed = -1.0;
    // True while Minimum calls setAnimationInterval itself, so its own call is not taken as the
    // game's request.
    bool g_applying = false;

    constexpr double kPreciseSpinSec = 0.001;

}

namespace minimum {

    // Makes the director use the interval the settings want. Called every frame, does nothing
    // unless the wanted value changed (for example when the player turns the unlock on).
    void applyFrameRateLimit() {
        if (g_requested < 0.0) return;

        auto const& cfg = config();
        bool const unlock = cfg.enabled && cfg.fpsUnlock;
        double const wanted = unlockedAnimationInterval(g_requested, unlock, cfg.fpsLimit);
        if (wanted == g_passed) return;

        g_applying = true;
        CCDirector::get()->setAnimationInterval(wanted);
        g_applying = false;
    }

    bool gameFrameCapOff() {
        return g_requested == 0.0;
    }

    // Holds the calling frame until `periodSec` has passed since `frameStart`. Sleeps for most of
    // the wait and spins the last millisecond. The sleep is only as exact as the OS timer: on
    // Windows the mod asks for 1 ms timer resolution (see system_tuning.cpp), which is why the
    // limiter is a choice and not on by default.
    void preciseFrameWait(std::chrono::steady_clock::time_point frameStart, double periodSec) {
#ifdef GEODE_IS_DESKTOP
        using clock = std::chrono::steady_clock;
        double const elapsed = std::chrono::duration<double>(clock::now() - frameStart).count();
        double const wait = frameWaitSeconds(elapsed, periodSec);
        if (wait <= 0.0) return;

        WaitSplit const split = splitWait(wait, kPreciseSpinSec);
        if (split.sleepSec > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(split.sleepSec));
        }
        auto const deadline = frameStart
            + std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(periodSec));
        while (clock::now() < deadline) {
            // Spin: the last millisecond is waited out exactly.
        }
#else
        (void)frameStart;
        (void)periodSec;
#endif
    }

}

struct FrameRateHook : Modify<FrameRateHook, CCDirector> {
    void setAnimationInterval(double interval) {
        if (!g_applying) g_requested = interval;

        auto const& cfg = minimum::config();
        double passed = interval;
        if (cfg.enabled && cfg.fpsUnlock) {
            passed = minimum::unlockedAnimationInterval(interval, true, cfg.fpsLimit);
        }
        g_passed = passed;
        CCDirector::setAnimationInterval(passed);
    }
};
