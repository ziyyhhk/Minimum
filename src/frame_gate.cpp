#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <minimum.hpp>
#include <minimum_logic.hpp>

using namespace geode::prelude;

// Frame gate
//
// Hooks CCDirector::drawScene. On every platform it measures frame times (FPS counter, 1% low,
// spike logger, adaptive particle cap), applies the game quality setting and lets the frame
// run normally. On Windows it can also decide per frame whether the scene is drawn or only
// updated ("logic-only" frame). That is used for:
//
//   * Background throttle: while the game window is not the foreground window, draw at a
//     low rate (default 20 fps). Nobody is looking, so the GPU is freed up.
//   * Draw Divide (opt-in): draw at "Visual FPS" while game logic keeps running at the
//     full frame rate. Same idea as the mat.draw-divide mod.
//
// A logic-only frame runs exactly what the proven Draw Divide implementation runs: scheduler
// update with the delta time the game's main loop already computed. calculateDeltaTime() is
// NOT called on those frames (calling it there creates a speed hack).
//
// Timing uses std::chrono::steady_clock and not the game's own delta time, so it does not
// depend on how the game's main loop is paced.
//
// The first 300 frames (loading) and frames with a pending scene switch are always drawn.
//
// A gap of more than one second between two frames is treated as "the game was away" (phone
// locked, app switched, window dragged). Those gaps are not counted as stutter and they restart
// the measurement windows, so coming back to the game never shows a fake FPS drop or makes the
// low latency guard back off.
//
// Adaptive cap window: only DRAWN frames are counted, a logic-only frame is cheap by design and
// would dilute the slow-frame ratio. A drawn frame is judged by the time since the previous
// DRAWN frame, against the target frame time or the gate's own interval (whichever is longer),
// see minimum::isSlowFrame. The step decision itself is minimum::AdaptiveCap (unit tested).
//
// Precise frame limiter (Windows / macOS, opt-in): at the end of a drawn frame, sleeps until the
// deadline and spins the last millisecond. Only while the game's own cap is off.
//
// The stats line is short and updated twice a second. A long CCLabelBMFont string rebuilds one
// sprite per character on every setString, which is itself a small periodic hitch.

namespace {

#ifdef GEODE_IS_MOBILE
    // Keep the line out of the notch / rounded corners.
    constexpr float kHudMargin = 24.f;
#else
    constexpr float kHudMargin = 4.f;
#endif

    class StatsHud : public CCNode {
    protected:
        CCLabelBMFont* m_label = nullptr;
        std::string m_lastText;

        bool init() override {
            if (!CCNode::init()) return false;

            m_label = CCLabelBMFont::create("Minimum", "chatFont.fnt");
            if (!m_label) return false;
            this->addChild(m_label);
            this->applyLayout();

            this->schedule(schedule_selector(StatsHud::tick), 0.5f);
            return true;
        }

        void applyLayout() {
            auto const& cfg = minimum::config();
            auto winSize = CCDirector::get()->getWinSize();

            bool right = cfg.hudCorner == minimum::HudCorner::TopRight
                      || cfg.hudCorner == minimum::HudCorner::BottomRight;
            bool bottom = cfg.hudCorner == minimum::HudCorner::BottomLeft
                       || cfg.hudCorner == minimum::HudCorner::BottomRight;

            m_label->setAnchorPoint({right ? 1.f : 0.f, bottom ? 0.f : 1.f});
            m_label->setPosition({
                right ? winSize.width - kHudMargin : kHudMargin,
                bottom ? kHudMargin : winSize.height - kHudMargin
            });
            // Stay on screen: the Detailed text is long, and HUD Scale goes up to 2.0. The size of the
            // label is that of the text set last, so a longer text is fitted on the next tick.
            auto const natural = m_label->getContentSize();
            m_label->setScale(minimum::hudFitScale(
                cfg.hudScale, natural.width, natural.height,
                winSize.width - 2.f * kHudMargin, winSize.height - 2.f * kHudMargin
            ));
            m_label->setOpacity(cfg.hudOpacity);
            m_label->setVisible(cfg.showStats);
        }

        void tick(float) {
            auto const& cfg = minimum::config();
            this->applyLayout();

            auto& counters = minimum::counters();
            // The worst frame of the last half second. Taking it resets it, so it is read here
            // even when the line is hidden and can not grow forever.
            uint32_t const worstUs = counters.worstFrameUs.exchange(0, std::memory_order_relaxed);
            if (!cfg.showStats) return;

            auto& stats = minimum::frameStats();
            uint32_t fps = counters.fps.load();
            bool throttled = counters.throttled.load();

            // The FPS number is measured with the wall clock inside the drawScene hook,
            // not derived from the scheduler, so it is the real number of frames drawn.
            char text[640];
            if (cfg.hudDetailed) {
                char const* sync = "";
                if (cfg.lowLatency) {
                    switch (static_cast<minimum::LatencyState>(counters.latencyState.load(std::memory_order_relaxed))) {
                        case minimum::LatencyState::Active: sync = " | sync on"; break;
                        case minimum::LatencyState::Waiting: sync = " | sync waiting"; break;
                        case minimum::LatencyState::Unavailable: sync = " | sync n/a"; break;
                        default: break;
                    }
                }
                uint32_t low = static_cast<uint32_t>(stats.lowFps(240) + 0.5);
                double frameMs = stats.averageFrameMs(60);

                // Line 1: the numbers a player compares with other counters.
                char line1[300];
#ifdef GEODE_IS_ANDROID
                // On Android the buffer swap happens after drawScene returns, so the CPU time is
                // the CPU work of the frame and the CPU-bound / GPU-bound readout means something.
                // On the other platforms drawScene includes the swap (vsync wait), so it is left out.
                double cpuMs = stats.averageCpuMs(60);
                auto const bottleneck = static_cast<minimum::Bottleneck>(counters.bottleneck.load(std::memory_order_relaxed));
                std::string_view const bottleneckText = minimum::bottleneckLabel(bottleneck);
                std::snprintf(
                    line1, sizeof(line1),
                    "%s%u FPS%s | low %u | %.1f ms | worst %.0f ms | cpu %.1f ms | logic %u/s | spikes %llu%s%s%.*s",
                    cfg.enabled ? "" : "(OFF) ", fps, throttled ? " (background)" : "",
                    low, frameMs, worstUs / 1000.0, cpuMs, counters.logicFps.load(),
                    static_cast<unsigned long long>(counters.spikes.load()), sync,
                    bottleneckText.empty() ? "" : " | ", static_cast<int>(bottleneckText.size()), bottleneckText.data()
                );
#else
                std::snprintf(
                    line1, sizeof(line1),
                    "%s%u FPS%s | low %u | %.1f ms | worst %.0f ms | logic %u/s | spikes %llu%s",
                    cfg.enabled ? "" : "(OFF) ", fps, throttled ? " (background)" : "",
                    low, frameMs, worstUs / 1000.0, counters.logicFps.load(),
                    static_cast<unsigned long long>(counters.spikes.load()), sync
                );
#endif

                // Line 2, the proof line: what Minimum is doing for you right now.
                char extra[48] = "";
                if (counters.thermal.load(std::memory_order_relaxed)) {
                    std::snprintf(extra, sizeof(extra), " | thermal");
                }
                char bench[32] = "";
                if (minimum::benchmarkActive()) {
                    double const nowSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    std::snprintf(bench, sizeof(bench), " | REC %.0f s", minimum::benchmarkElapsed(nowSeconds));
                }
                char line2[200];
                std::snprintf(
                    line2, sizeof(line2),
                    "skipped %u/s | culled %u/s | capped %u/s | cap step %d (%u) | trips %u%s%s",
                    counters.skippedPerSec.load(std::memory_order_relaxed),
                    counters.culledPerSec.load(std::memory_order_relaxed),
                    counters.cappedPerSec.load(std::memory_order_relaxed),
                    minimum::currentCapStep(), minimum::effectiveParticleCap(),
                    counters.latencyTrips.load(std::memory_order_relaxed), extra, bench
                );

                // Line 3: totals since the game started.
                char line3[200];
                std::snprintf(
                    line3, sizeof(line3),
                    "drawn %llu | logic-only %llu | idle skipped %llu | culled %llu | pools capped %llu | budgeted %llu",
                    static_cast<unsigned long long>(counters.framesDrawn.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(counters.framesLogicOnly.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(counters.idleParticleDrawsSkipped.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(counters.particleDrawsCulled.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(counters.particlePoolsCapped.load(std::memory_order_relaxed)),
                    static_cast<unsigned long long>(counters.spawnsBudgeted.load(std::memory_order_relaxed))
                );

                std::snprintf(text, sizeof(text), "%s\n%s\n%s", line1, line2, line3);
            }
            else {
                // When the game updates its logic at a clearly different rate than it draws
                // frames, show both, so the number can be compared with other FPS counters.
                char logicText[24] = "";
                uint32_t const logic = counters.logicFps.load(std::memory_order_relaxed);
                if (minimum::logicRateDiffers(fps, logic)) {
                    std::snprintf(logicText, sizeof(logicText), " | logic %u", logic);
                }
                std::snprintf(
                    text, sizeof(text), "%s%u FPS%s%s",
                    cfg.enabled ? "" : "(OFF) ", fps, throttled ? " (background)" : "", logicText
                );
            }

            // Green when it holds the target, yellow when it does not quite, red when bad.
            // Before the first measurement the number is 0: that is "no data yet", not "bad".
            if (fps == 0) {
                m_label->setColor(ccc3(200, 200, 200));
            }
            else {
                switch (minimum::classifyFps(fps, cfg.targetFps)) {
                    case minimum::FpsBand::Good: m_label->setColor(ccc3(120, 255, 120)); break;
                    case minimum::FpsBand::Okay: m_label->setColor(ccc3(255, 225, 90)); break;
                    case minimum::FpsBand::Bad: m_label->setColor(ccc3(255, 100, 100)); break;
                }
            }

            // setString rebuilds the whole label, only do it when the text changed.
            if (m_lastText != text) {
                m_lastText = text;
                m_label->setString(text);
            }
        }

    public:
        static StatsHud* create() {
            auto* ret = new StatsHud();
            if (ret && ret->init()) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }
    };

    bool g_hudCreated = false;

}

namespace minimum {

    void ensureHud(unsigned int totalFrames) {
        if (g_hudCreated || totalFrames < 200) return;
        if (!config().showStats) return;

        auto* overlay = OverlayManager::get();
        if (!overlay) return;
        if (auto* hud = StatsHud::create()) {
            overlay->addChild(hud, 99999);
            g_hudCreated = true;
        }
    }

}

struct FrameGate : Modify<FrameGate, CCDirector> {
    void drawScene() {
        using clock = std::chrono::steady_clock;
        static clock::time_point s_last = clock::now();
        static clock::time_point s_lastDrawn = clock::now();
        static clock::time_point s_lastSpikeLog{};
        static double s_accum = 0.0;
        static uint64_t s_createdSeen = 0;
        static clock::time_point s_windowStart = clock::now();
        static uint32_t s_windowFrames = 0;
        static uint32_t s_windowSlow = 0;
        static uint32_t s_fpsDrawn = 0;
        static uint32_t s_fpsAll = 0;
        static uint64_t s_updatesSeen = 0;
        static clock::time_point s_fpsStart = clock::now();
        static minimum::LatencyGate s_latencyGate;
        static bool s_syncOk = false;
        static uint64_t s_skipSeen = 0;
        static uint64_t s_cappedSeen = 0;
        static uint64_t s_culledSeen = 0;
#ifdef GEODE_IS_ANDROID
        static clock::time_point s_thermalNext{};
#endif
#ifdef GEODE_IS_DESKTOP
        static double s_deadline = 0.0;   // precise frame limiter, seconds on the steady clock
#endif

        auto const now = clock::now();
        double const nowSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
        double const rawDelta = std::chrono::duration<double>(now - s_last).count();
        s_last = now;
        // After a hitch (alt-tab, level load) do not try to "catch up" with a burst of draws.
        double const wallDelta = std::min(rawDelta, 0.25);

        minimum::refreshConfigIfDue(now);
        auto const& cfg = minimum::config();
        auto& counters = minimum::counters();

        // One tick per call of this hook, drawn or not: the spawn budget uses it to tell frames
        // apart. framesInLevel counts calls that STARTED with a level open, so the frame that
        // loads a level never counts (see kSpawnBudgetGraceFrames). Only looked up when needed.
        counters.frameIndex.fetch_add(1, std::memory_order_relaxed);
        if (cfg.spawnBudget > 0 && PlayLayer::get()) {
            uint32_t const inLevel = counters.framesInLevel.load(std::memory_order_relaxed);
            if (inLevel < 1000000u) counters.framesInLevel.store(inLevel + 1, std::memory_order_relaxed);
        }
        else {
            counters.framesInLevel.store(0, std::memory_order_relaxed);
        }

        unsigned int totalFrames = this->getTotalFrames();
        bool const focused = minimum::processHasForeground(now);

        // The game was away (phone locked, app switched, window dragged). Start every
        // measurement over so that gap is not mistaken for stutter.
        bool const resumed = rawDelta > 1.0;
        if (resumed) {
            s_fpsDrawn = 0;
            s_fpsAll = 0;
            s_fpsStart = now;
            s_windowStart = now;
            s_windowFrames = 0;
            s_windowSlow = 0;
            s_lastDrawn = now;
            s_updatesSeen = counters.schedulerUpdates.load(std::memory_order_relaxed);
            s_skipSeen = counters.idleParticleDrawsSkipped.load(std::memory_order_relaxed);
            s_cappedSeen = counters.particlePoolsCapped.load(std::memory_order_relaxed);
            s_culledSeen = counters.particleDrawsCulled.load(std::memory_order_relaxed);
#ifdef GEODE_IS_DESKTOP
            s_deadline = 0.0;
#endif
            minimum::frameStats().clear();
        }
        bool const measurable = totalFrames >= 300 && focused && !resumed;

        // Game quality (low detail mode), only does something when the setting changed.
        minimum::applyGameQuality();

        // ---- Tab-out volume (Windows) ----------------------------------------------
        // Called every frame, stateless: it makes the volume match the focus state now.
        minimum::audioPoll(focused);

        // ---- Measured FPS (wall clock, twice a second) --------------------------------
        ++s_fpsAll;
        {
            double const span = std::chrono::duration<double>(now - s_fpsStart).count();
            if (span >= 0.5) {
                double const fps = s_fpsDrawn / span;
                counters.fps.store(static_cast<uint32_t>(fps + 0.5));

                // Game logic steps per second, counted by the CCScheduler::update hook. If that
                // hook has not run (it should always run), fall back to the loop iterations.
                uint64_t const updates = counters.schedulerUpdates.load(std::memory_order_relaxed);
                uint64_t const delta = updates >= s_updatesSeen ? updates - s_updatesSeen : 0;
                s_updatesSeen = updates;
                double const logic = delta > 0 ? static_cast<double>(delta) / span : s_fpsAll / span;
                counters.logicFps.store(static_cast<uint32_t>(logic + 0.5));

                // Low latency mode only runs while the game is holding its frame rate, and
                // backs off for a growing amount of time when it costs frames (see
                // minimum::LatencyGate). It is not allowed to flip on and off every window.
                bool const wantSync = cfg.enabled && cfg.lowLatency && focused && totalFrames >= 300;
                s_syncOk = s_latencyGate.update(fps, nowSeconds, wantSync);
                counters.latencyTrips.store(static_cast<uint32_t>(std::max(0, s_latencyGate.trips())), std::memory_order_relaxed);

                // Proof line numbers: how much each optimization did in the last window.
                uint64_t const skipped = counters.idleParticleDrawsSkipped.load(std::memory_order_relaxed);
                uint64_t const capped = counters.particlePoolsCapped.load(std::memory_order_relaxed);
                uint64_t const culled = counters.particleDrawsCulled.load(std::memory_order_relaxed);
                counters.skippedPerSec.store(minimum::ratePerSecond(skipped >= s_skipSeen ? skipped - s_skipSeen : 0, span), std::memory_order_relaxed);
                counters.cappedPerSec.store(minimum::ratePerSecond(capped >= s_cappedSeen ? capped - s_cappedSeen : 0, span), std::memory_order_relaxed);
                counters.culledPerSec.store(minimum::ratePerSecond(culled >= s_culledSeen ? culled - s_culledSeen : 0, span), std::memory_order_relaxed);
                s_skipSeen = skipped;
                s_cappedSeen = capped;
                s_culledSeen = culled;

#ifdef GEODE_IS_ANDROID
                // CPU-bound / GPU-bound. Android only: there the buffer swap happens after
                // drawScene returns, so the stored CPU time really is CPU work (see below).
                {
                    bool const syncActive = counters.latencyState.load(std::memory_order_relaxed)
                        == static_cast<uint8_t>(minimum::LatencyState::Active);
                    counters.bottleneck.store(
                        static_cast<uint8_t>(minimum::bottleneckFor(minimum::frameStats(), syncActive)),
                        std::memory_order_relaxed
                    );
                }

                // Thermal status: asks a system service, so only every 2 seconds.
                {
                    using namespace std::chrono_literals;
                    if (now >= s_thermalNext) {
                        s_thermalNext = now + 2s;
                        int const status = (cfg.enabled && cfg.capParticles) ? minimum::readThermalStatus() : -1;
                        minimum::noteThermalStatus(status, nowSeconds);
                    }
                }
#endif

                // Android / iOS "Unlock FPS": re-applies the frame interval when the setting changed.
                minimum::applyFrameUnlock();

                s_fpsDrawn = 0;
                s_fpsAll = 0;
                s_fpsStart = now;
            }
        }

        // ---- Adaptive particle cap window (once per second) -----------------------
        // The counts are filled after a frame is DRAWN (below), never here: logic-only frames
        // return before that and so do not dilute the slow-frame ratio.
        {
            using namespace std::chrono_literals;
            if (now - s_windowStart >= 1s) {
                minimum::noteFrameWindow(s_windowFrames, s_windowSlow);
                s_windowStart = now;
                s_windowFrames = 0;
                s_windowSlow = 0;
            }
        }

        // ---- Frame time tracking and spike logger --------------------------------
        // Skipped while loading (first 300 frames), while unfocused and after the game was
        // away, where long gaps are expected.
        if (measurable) {
            uint32_t us = static_cast<uint32_t>(std::min(rawDelta * 1e6, 4.0e9));
            if (us > counters.worstFrameUs.load(std::memory_order_relaxed)) {
                counters.worstFrameUs.store(us, std::memory_order_relaxed);
            }

            double ms = rawDelta * 1000.0;
            if (cfg.spikeLogger && ms >= cfg.spikeThresholdMs) {
                ++counters.spikes;

                // Rate limit the log so a long freeze or a bad stretch can not flood it.
                using namespace std::chrono_literals;
                if (now - s_lastSpikeLog >= 250ms) {
                    s_lastSpikeLog = now;

                    uint64_t created = counters.particleSystemsCreated.load();
                    uint64_t createdDuringFrame = created - s_createdSeen;

                    auto* pl = PlayLayer::get();
                    if (pl && pl->m_player1) {
                        log::warn(
                            "Frame spike: {:.1f} ms | in level, player x {:.0f} | particle systems created during the frame: {}",
                            ms, pl->m_player1->getPositionX(), createdDuringFrame
                        );
                    }
                    else {
                        log::warn(
                            "Frame spike: {:.1f} ms | not in a level | particle systems created during the frame: {}",
                            ms, createdDuringFrame
                        );
                    }
                }
            }
        }
        s_createdSeen = counters.particleSystemsCreated.load();

        // Seconds the gate wants between two drawn frames (0 = no gate). Also tells the slow frame
        // check and the precise limiter that draws are being paced on purpose.
        double gateInterval = 0.0;

#ifdef GEODE_IS_WINDOWS
        // ---- Frame gate ----------------------------------------------------------
        // Seconds between two drawn frames. 0 means: draw every frame.
        double interval = 0.0;
        if (cfg.enabled) {
            if (cfg.backgroundThrottle && !focused) {
                interval = 1.0 / cfg.backgroundFps;
            }
            else if (cfg.drawDivide) {
                interval = 1.0 / cfg.visualFps;
            }
        }

        gateInterval = interval;
        counters.throttled.store(cfg.enabled && cfg.backgroundThrottle && !focused);

        bool mustDraw = interval <= 0.0 || totalFrames < 300 || this->getNextScene() != nullptr;

        if (!mustDraw) {
            s_accum += wallDelta;
            if (s_accum < interval) {
                // Logic-only frame: no glClear, no scene visit, no buffer swap.
                if (!this->isPaused()) {
                    this->getScheduler()->update(this->getDeltaTime());
                }
                ++counters.framesLogicOnly;
                return;
            }
            s_accum -= interval;
            // Fell more than one interval behind: drop the backlog instead of drawing in a burst.
            if (s_accum > interval) s_accum = 0.0;
        }
        else {
            s_accum = 0.0;
        }
#else
        counters.throttled.store(false);
        (void)wallDelta;
        (void)s_accum;
#endif

        auto const drawStart = clock::now();
        CCDirector::drawScene();
        auto const drawEnd = clock::now();
        ++counters.framesDrawn;
        ++s_fpsDrawn;

        // Per drawn frame: how long since the previous drawn frame (what the player sees) and
        // how long this call took. On Android the buffer swap happens after drawScene returns,
        // so that second number is the CPU work of the frame, which is what the CPU hint needs.
        if (measurable) {
            double const frameMs = std::chrono::duration<double, std::milli>(drawStart - s_lastDrawn).count();
            double const workMs = std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();
            minimum::frameStats().push(std::min(frameMs, 1000.0), workMs);

            // Adaptive cap window: this frame was drawn, so it counts.
            ++s_windowFrames;
            if (cfg.adaptiveCap && minimum::isSlowFrame(frameMs, cfg.targetFps, gateInterval)) {
                ++s_windowSlow;
            }

            // One atomic load while no benchmark runs.
            minimum::benchmarkFrame(nowSeconds, frameMs);
            minimum::reportCpuWork(
                std::chrono::duration_cast<std::chrono::nanoseconds>(drawEnd - drawStart).count()
            );
        }
        else {
            minimum::reportCpuWork(0);
        }
        s_lastDrawn = drawStart;

        // Low latency mode: let the GPU catch up before the next frame starts, so input is
        // not stuck behind frames queued in the driver.
        {
            using minimum::LatencyState;
            LatencyState state = LatencyState::Off;
            if (cfg.enabled && cfg.lowLatency) {
                if (!minimum::hardGpuSyncAvailable()) {
                    state = LatencyState::Unavailable;
                }
                else if (s_syncOk && focused && totalFrames >= 300) {
                    state = LatencyState::Active;
                    minimum::hardGpuSync();
                }
                else {
                    state = LatencyState::Waiting;
                }
            }
            counters.latencyState.store(static_cast<uint8_t>(state), std::memory_order_relaxed);
        }

        minimum::ensureHud(totalFrames);

#ifdef GEODE_IS_DESKTOP
        // ---- Precise frame limiter (Windows / macOS, opt-in) ---------------------------
        // Sleep until the frame deadline, then spin the last millisecond, so frames come out
        // exactly 1 / Target FPS apart. Not while the gate paces the draws, not while loading or
        // unfocused, and not when the game's own cap is already holding the target (two
        // limiters on top of each other make frame times jitter).
        if (cfg.enabled && cfg.preciseFrameLimit && focused && totalFrames >= 300 && gateInterval <= 0.0
            && minimum::shouldPreciseLimit(true, cfg.targetFps, this->getAnimationInterval())) {
            double const frameInterval = 1.0 / cfg.targetFps;
            auto const secondsNow = [] {
                return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
            };
            double const deadline = minimum::nextDeadline(s_deadline, secondsNow(), frameInterval);
            auto const plan = minimum::planWait(secondsNow(), deadline, 0.001);
            if (plan.sleepSeconds > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(plan.sleepSeconds));
            }
            while (secondsNow() < deadline) {
                // spin: the OS wake up is not exact, the last millisecond is waited out here
            }
            s_deadline = deadline;
        }
        else {
            s_deadline = 0.0;
        }
#endif
    }
};
