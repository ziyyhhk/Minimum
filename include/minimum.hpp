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

    enum class HudCorner : uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

    // 0 = Normal, 1 = Above Normal, 2 = High
    enum class ProcessPriority : uint8_t { Normal = 0, AboveNormal = 1, High = 2 };

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

        // System (Windows only, ignored elsewhere)
        bool timerResolution = true;
        bool disablePowerThrottling = true;
        ProcessPriority priority = ProcessPriority::Normal;

        // Adaptive particle cap (all platforms)
        bool adaptiveCap = false;
        double adaptiveTargetFps = 60.0;

        // Diagnostics
        bool spikeLogger = true;
        double spikeThresholdMs = 40.0;

        // Stats line
        bool showStats = true;
        bool hudDetailed = false;
        HudCorner hudCorner = HudCorner::TopLeft;
        float hudScale = 0.5f;
        uint8_t hudOpacity = 200;

        // Latency (desktop: Windows and macOS)
        bool lowLatency = false;

        // Other
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
        std::atomic<uint64_t> spikes{0};
        // Longest gap between two frames since the stats line last read it (microseconds).
        std::atomic<uint32_t> worstFrameUs{0};
        // Measured with the wall clock inside the drawScene hook, refreshed twice a second.
        // fps = frames actually drawn per second, logicFps = game updates per second.
        std::atomic<uint32_t> fps{0};
        std::atomic<uint32_t> logicFps{0};
        // True while the background throttle is limiting the draw rate.
        std::atomic<bool> throttled{false};
    };

    Config const& config();
    Counters& counters();

    // Re-read every setting from Mod::get().
    void refreshConfig();

    // Cheap: only re-reads settings if 500 ms passed since the last refresh.
    void refreshConfigIfDue(std::chrono::steady_clock::time_point now);

    // True unless the game window is positively known to be unfocused (Windows).
    // Polls at most every 250 ms and needs two unfocused polls in a row before it says
    // "unfocused", so a false alarm can never throttle the game or dim the audio.
    // Always true on platforms without an implementation.
    bool processHasForeground(std::chrono::steady_clock::time_point now);

    // Called by the frame hook every frame with the current focus state (Windows only,
    // no-op elsewhere). Dims the audio while unfocused and restores it as soon as the
    // window is focused again. Stateless on purpose: if a restore ever fails it is simply
    // retried on the next frame, so the volume can not get stuck low.
    void audioPoll(bool focused);

    // Hard GPU sync (glFinish) after a drawn frame. Trims the queue of frames the driver
    // keeps in flight, which lowers input-to-screen latency. Windows and macOS only.
    void hardGpuSync();

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
