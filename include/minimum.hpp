#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

// Minimum: shared state.
//
// Hooks in this mod run on the main (render) thread, some of them hundreds of
// times per frame. Mod::getSettingValue does a string keyed lookup on every
// call, so hooks read this plain snapshot instead. The snapshot is refreshed
// on load and then about twice a second from the CCDirector::drawScene hook.

namespace minimum {

    struct Config {
        bool enabled = true;

        // Particles
        bool skipIdleParticles = true;
        bool capParticles = true;
        unsigned int particleCap = 128;

        // Background (game window not focused)
        bool backgroundThrottle = true;
        double backgroundFps = 20.0;
        bool backgroundVolume = true;
        float backgroundVolumeScale = 0.1f;

        // Render
        bool drawDivide = false;
        double visualFps = 60.0;

        // Other
        bool fastAltTab = true;
        bool showStats = true;
    };

    // Proof-of-work counters shown by the stats overlay. Only touched from the
    // main thread, atomics are just there so a stray thread can not corrupt them.
    struct Counters {
        std::atomic<uint64_t> framesDrawn{0};
        std::atomic<uint64_t> framesLogicOnly{0};
        std::atomic<uint64_t> idleParticleDrawsSkipped{0};
        std::atomic<uint64_t> particlePoolsCapped{0};
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

}
