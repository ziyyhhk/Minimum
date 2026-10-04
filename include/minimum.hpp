#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

// Minimum: shared state.
//
// Hooks in this mod run on the main (render) thread, some of them hundreds of
// times per frame. Mod::getSettingValue does a string keyed lookup on every
// call, so hooks read this plain snapshot instead. The snapshot is refreshed
// on load and then about twice a second from the CCDirector::drawScene hook.

namespace minimum {

    enum class HudCorner {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    enum class ProcessPriority {
        Normal,
        AboveNormal,
        High
    };

    struct Config {
        bool enabled = true;

        // Particles
        bool skipIdleParticles = true;
        bool capParticles = true;
        unsigned int particleCap = 128;

        // Background (game window not focused) — Windows only
        bool backgroundThrottle = true;
        double backgroundFps = 20.0;
        bool backgroundVolume = true;
        float backgroundVolumeScale = 0.1f;

        // Render — Windows only
        bool drawDivide = false;
        double visualFps = 60.0;

        // System — Windows only
        bool timerResolution = true;
        bool disablePowerThrottling = true;
        ProcessPriority priority = ProcessPriority::Normal;

        // Adaptive particle cap (all platforms)
        bool adaptiveCap = false;
        double adaptiveTargetFps = 60.0;

        // Diagnostics
        bool spikeLogger = true;
        double spikeThresholdMs = 40.0;

        // Stats HUD
        bool showStats = true;
        bool hudDetailed = false;
        HudCorner hudCorner = HudCorner::TopLeft;
        float hudScale = 0.5f;
        uint8_t hudOpacity = 200;

        // Other — Windows only
        bool fastAltTab = true;
    };

    // Proof-of-work counters shown by the stats overlay. Only touched from the
    // main thread, atomics are just there so a stray thread can not corrupt them.
    struct Counters {
        std::atomic<uint64_t> framesDrawn{0};
        std::atomic<uint64_t> framesLogicOnly{0};
        std::atomic<uint64_t> idleParticleDrawsSkipped{0};
        std::atomic<uint64_t> particlePoolsCapped{0};
        std::atomic<uint64_t> particleSystemsCreated{0};
        std::atomic<uint64_t> frameSpikes{0};
        std::atomic<double> worstFrameMs{0.0};
    };

    Config const& config();
    Counters& counters();

    // Re-read every setting from Mod::get().
    void refreshConfig();

    // Cheap: only re-reads settings if 500 ms passed since the last refresh.
    void refreshConfigIfDue(std::chrono::steady_clock::time_point now);

    // True when a window of this process is the foreground window.
    // Polls at most every 250 ms. Always true on platforms without an
    // implementation (macOS), so background throttling simply never triggers there.
    bool processHasForeground(std::chrono::steady_clock::time_point now);

    // Called by the frame hook when the window gains or loses focus (Windows only,
    // where focus is polled reliably). Dims / restores the audio. No-op elsewhere.
    void onFocusChanged(bool focused);

    // Particle cap that is actually applied right now: the configured cap, lowered
    // in steps by the adaptive cap when the game cannot hold the target FPS.
    unsigned int effectiveParticleCap();

    // Fed once per second by the frame hook: how many frames were measured and how
    // many of them were slow (longer than 1.5x the target frame time).
    void noteFrameWindow(uint32_t frames, uint32_t slowFrames);

    // Windows only: 1 ms timer resolution, opt out of power throttling, process
    // priority. Only does work when the wanted state differs from the applied
    // state, so it is safe to call after every config refresh. No-op elsewhere.
    void applySystemTuning();

}
