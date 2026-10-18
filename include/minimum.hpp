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
        bool cullOffscreenParticles = true;
        // Particle systems per frame that get a full pool during a level. 0 = off.
        unsigned int spawnBudget = 0;

        // Background (game window not focused)
        bool backgroundThrottle = true;
        double backgroundFps = 20.0;
        bool backgroundVolume = true;
        float backgroundVolumeScale = 0.1f;

        // Render
        bool drawDivide = false;
        double visualFps = 60.0;

        // Frame rate limit (Android and iOS)
        bool fpsUnlock = false;
        int fpsLimit = 120;

        // Frame pacing (Windows and macOS): wait out the rest of each frame when the game's own
        // cap is off. See preciseFrameWait.
        bool preciseFrameLimit = false;

        // System (Windows only, ignored elsewhere)
        bool timerResolution = true;
        bool disablePowerThrottling = true;
        ProcessPriority priority = ProcessPriority::Normal;

        // Adaptive particle cap (all platforms). On Android it also drops while the phone is hot.
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

        // Other
        bool fastAltTab = true;
    };

    // Proof-of-work counters shown by the stats overlay. Only touched from the
    // main thread, atomics are just there so a stray thread can not corrupt them.
    struct Counters {
        std::atomic<uint64_t> framesDrawn{0};
        std::atomic<uint64_t> framesLogicOnly{0};
        std::atomic<uint64_t> idleParticleDrawsSkipped{0};
        std::atomic<uint64_t> offscreenParticleDrawsSkipped{0};
        std::atomic<uint64_t> particlePoolsCapped{0};
        std::atomic<uint64_t> particleSystemsCreated{0};
        std::atomic<uint64_t> spikes{0};
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

        // The last half second, written by the frame hook when its window closes.
        std::atomic<uint32_t> skipsPerSec{0};      // idle + off-screen draws skipped
        std::atomic<uint32_t> cappedPerSec{0};     // particle systems that got a smaller pool
        std::atomic<uint32_t> logicOnlyPerSec{0};  // frames that only ran the game logic
        std::atomic<uint32_t> capStep{0};          // combined adaptive + thermal step, 0..3
        std::atomic<uint32_t> thermalStep{0};      // thermal part of capStep, 0 = not hot
        std::atomic<uint32_t> latencyTrips{0};     // trips on record in the low latency guard
        // Bottleneck::None / CpuBound / GpuBound from minimum_logic.hpp, stored as a number.
        std::atomic<uint8_t> bottleneck{0};

        // Particle work, only measured while the detailed stats line is on. Running totals in
        // nanoseconds, so the frame hook can take the difference per window.
        std::atomic<uint64_t> particleUpdateNs{0};
        std::atomic<uint64_t> particleDrawNs{0};
        // Particle update + draw as a share of the CPU work of the frames in the last half
        // second, 0..100. Approximate: logic-only frames add particle time without drawn work.
        std::atomic<uint32_t> particleShare{0};
    };

    // Off: the setting is off. Waiting: on, but not syncing right now (loading, or it backed
    // off because FPS dropped). Active: keeping the frame queue short. Unavailable: this device
    // has no GL sync objects we could find, so the mode can not do anything.
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

    // Frame queue bound for Low Latency Mode (see latency.cpp). frameQueueWait runs before a
    // frame is drawn, frameQueueSubmit after it, and frameQueueReset when the mode is not running.
    void frameQueueWait();
    void frameQueueSubmit();
    void frameQueueReset();

    // False when the GL sync object functions could not be found on this device.
    // The lookup runs on first call and is logged.
    bool hardGpuSyncAvailable();

    // The round Minimum logo as a sprite: the packaged sprite when it is there, otherwise the
    // copy built into the mod. Null only if both fail. Never the pink "missing texture" square.
    cocos2d::CCSprite* createLogoSprite();

    // Particle cap that is actually applied right now: the configured cap, lowered
    // in steps by the adaptive cap and the thermal response when the game cannot hold the
    // target FPS or the phone is hot.
    unsigned int effectiveParticleCap();

    // The combined cap step (0..3) the effective cap uses right now.
    int currentCapStep();

    // The thermal part of the cap step (0..3). 0 when the phone is not hot or the thermal
    // response is off (it only exists on Android).
    int thermalCapStep();

    // True when the game asked for no frame interval limit, which is when Minimum's own precise
    // limiter may pace frames. False until the game has asked for anything.
    bool gameFrameCapOff();

    // Fed once per second by the frame hook: how many drawn frames were measured and how
    // many of them were slow (longer than 1.5x the target frame time).
    void noteFrameWindow(uint32_t frames, uint32_t slowFrames);

    // Marks the start of one drawScene call. The spawn budget counts the particle systems
    // created since then. Called by the frame hook before anything else.
    void particleFrameBegin();

    // Android only: reads the thermal status about once a second and feeds the thermal
    // governor. No-op elsewhere.
    void pollThermal(double nowSec);

    // Android only: the thermal status (NDK AThermalStatus, -1 when unknown) as the thermal
    // governor sees it. Called by pollThermal.
    void noteThermalStatus(int status, double nowSec);

    // Mobile only: asks the OS for the frame interval the settings want (see the frame rate
    // unlock). Called every frame, does nothing unless the wanted value changed.
    void applyFrameRateLimit();

    // Windows and macOS: waits until `periodSec` has passed since `frameStart`. Sleeps for
    // most of the wait and spins the last millisecond. No-op elsewhere.
    void preciseFrameWait(std::chrono::steady_clock::time_point frameStart, double periodSec);

    // Benchmark: called by the frame hook for every measured drawn frame. Writes the CSV when
    // a recording ends.
    void noteBenchmarkFrame(double nowSec, double frameMs, double cpuMs);

    // Benchmark: a quick tap on the live card of the popup (five in two seconds) starts a 60 s
    // recording, or stops one early.
    void noteSecretTap(double nowSec);

    // Seconds left in the running benchmark, 0 when none runs.
    double benchmarkRemainingSec(double nowSec);

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
