#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <minimum_logic.hpp>

namespace cocos2d { class CCSprite; }

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
        // Skip the draw (never the update) of particle systems that are completely off screen.
        bool cullOffscreenParticles = true;
        // Most particle systems the game may create per frame during a level, 0 = off. Systems
        // beyond it are still created, with a tiny pool (see spawnBudgetTotal).
        int spawnBudget = 0;

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
        bool adaptiveCap = true;

        // The frame rate the player aims at (screen refresh rate). Used by the adaptive cap,
        // the FPS counter colors and the Android CPU hint.
        double targetFps = 60.0;

        // Diagnostics
        bool spikeLogger = false;
        double spikeThresholdMs = 40.0;

        // Stats line
        bool showStats = true;
        bool hudDetailed = false;
        HudCorner hudCorner = HudCorner::TopLeft;
        float hudScale = 0.5f;
        uint8_t hudOpacity = 200;

        // Latency (all platforms)
        bool lowLatency = false;

        // Turns on the game's own Low Detail Mode while Minimum is on.
        bool lowDetail = false;

        // Android only: tell the system how much CPU time each frame needs.
        bool androidCpuHint = true;

        // Android / iOS only (not read elsewhere): ask the system for a higher frame rate.
        bool fpsUnlock = false;
        int fpsLimit = 120;

        // Windows / macOS only (not read elsewhere): precise frame limiter.
        bool preciseFrameLimit = false;

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
        // Draws skipped because the system was completely off screen (cull-offscreen-particles).
        std::atomic<uint64_t> particleDrawsCulled{0};
        // Particle systems created over the per-frame spawn budget (they got a tiny pool).
        std::atomic<uint64_t> spawnsBudgeted{0};
        std::atomic<uint64_t> spikes{0};
        // Counts one per call of the frame hook, drawn or not. Used to tell "this frame" from
        // the next one (spawn budget).
        std::atomic<uint64_t> frameIndex{0};
        // Frame hook calls in a row that started with a level open (0 outside a level). The spawn
        // budget waits for this to reach kSpawnBudgetGraceFrames so it never touches level loading.
        std::atomic<uint32_t> framesInLevel{0};
        // Longest gap between two measured frames since the stats line last read it
        // (microseconds). The stats line takes it with exchange(0) twice a second, so the value
        // shown is the worst frame of the last half second.
        std::atomic<uint32_t> worstFrameUs{0};
        // Per second, refreshed twice a second with the fps number (see ratePerSecond).
        std::atomic<uint32_t> skippedPerSec{0};
        std::atomic<uint32_t> cappedPerSec{0};
        std::atomic<uint32_t> culledPerSec{0};
        // Low Latency Mode backoff trips (LatencyGate::trips, decays after 2 minutes clean).
        std::atomic<uint32_t> latencyTrips{0};
        // What the bottleneck readout says right now (a minimum::Bottleneck).
        std::atomic<uint8_t> bottleneck{0};
        // True while the thermal guard is lowering the particle cap (Android).
        std::atomic<bool> thermal{false};
        // Measured with the wall clock inside the drawScene hook, refreshed twice a second.
        // fps = frames actually drawn per second, logicFps = game updates per second.
        std::atomic<uint32_t> fps{0};
        // Scheduler updates (game logic steps) per second, counted by a hook on
        // CCScheduler::update. Falls back to the number of frames when that hook has not run.
        std::atomic<uint32_t> logicFps{0};
        // Raw number of scheduler updates so far (only the hook writes it).
        std::atomic<uint64_t> schedulerUpdates{0};
        // True while the background throttle is limiting the draw rate.
        std::atomic<bool> throttled{false};
        // What Low Latency Mode is doing right now (see LatencyState). Written by the frame
        // hook, read by the FPS counter and the popup so the player can SEE whether it runs.
        std::atomic<uint8_t> latencyState{0};
    };

    // Off: the setting is off. Waiting: on, but not syncing right now (loading, or it backed
    // off because FPS dropped). Active: syncing every frame. Unavailable: this device has no
    // glFinish we could find, so the mode can not do anything.
    enum class LatencyState : uint8_t { Off = 0, Waiting = 1, Active = 2, Unavailable = 3 };

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
    // keeps in flight, which can lower input-to-screen latency. Does nothing if the GL
    // library has no glFinish. The lookup happens once, on first use, and is logged.
    void hardGpuSync();

    // False when no glFinish could be found on this device (the lookup runs on first call).
    bool hardGpuSyncAvailable();

    // The round Minimum logo as a sprite: the packaged sprite when it is there, otherwise the
    // copy built into the mod. Null only if both fail. Never the pink "missing texture" square.
    cocos2d::CCSprite* createLogoSprite();

    // Particle cap that is actually applied right now: the configured cap, lowered
    // in steps by the adaptive cap when the game cannot hold the target FPS.
    unsigned int effectiveParticleCap();

    // Fed once per second by the frame hook: how many DRAWN frames were measured and how many
    // of them were slow (see slowFrameThresholdMs). Logic-only frames are not counted.
    void noteFrameWindow(uint32_t drawnFrames, uint32_t slowFrames);

    // The step the particle cap is at right now (0 = the configured cap), adaptive cap and
    // thermal guard together. For the stats line.
    int currentCapStep();

    // Fed every couple of seconds by the frame hook with the device's thermal status (see
    // ThermalGuard for the values; -1 = unknown) and the current time in seconds.
    void noteThermalStatus(int status, double nowSeconds);

    // Android 11+ (API 30): the device's thermal status through AThermal_*, resolved at runtime.
    // -1 when it is not available (older Android, other platforms). Not free (it asks a system
    // service), so call it every few seconds, never every frame.
    int readThermalStatus();

    // Android / iOS: makes the system frame interval match the "Unlock FPS" settings. Only does
    // work when the wanted state changed, safe to call after every config refresh. No-op elsewhere.
    void applyFrameUnlock();

    // Benchmark mode: records 60 s of frame times to a CSV in the mod's save directory.
    // benchmarkToggle() starts a run (or cancels the one that is running). benchmarkFrame() is
    // called by the frame hook for every drawn frame and does nothing when no run is active.
    bool benchmarkActive();
    void benchmarkToggle();
    void benchmarkFrame(double nowSeconds, double frameMs);
    // Seconds into the running benchmark, 0 when none runs.
    double benchmarkElapsed(double nowSeconds);

    // Windows only: 1 ms timer resolution, opt out of power throttling, process
    // priority. Only does work when the wanted state differs from the applied
    // state, so it is safe to call after every config refresh. No-op elsewhere.
    void applySystemTuning();

    // Rolling frame times of the drawn frames (see FrameStats). Main thread only.
    FrameStats& frameStats();

    // Leaves the active preset without changing how the game behaves: the preset's numbers
    // are copied into the individual settings and the preset becomes "Custom". Called before
    // the player flips a toggle that a preset controls, otherwise the preset would silently
    // undo the click.
    void leavePreset();

    // Applies the "Low detail mode" setting to the game (and puts it back when it is switched
    // off). Cheap when nothing changed, safe to call every frame once the game is running.
    void applyGameQuality();

    // Android only: reports how long the CPU was busy in the frame that just finished so the
    // system can pick a fitting CPU speed ahead of time. No-op elsewhere.
    void reportCpuWork(int64_t workNs);

    // Creates the FPS counter overlay once the game has been running for a while.
    void ensureHud(unsigned int totalFrames);

}
