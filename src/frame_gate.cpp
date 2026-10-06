#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
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
            m_label->setScale(cfg.hudScale);
            m_label->setOpacity(cfg.hudOpacity);
            m_label->setVisible(cfg.showStats);
        }

        void tick(float) {
            auto const& cfg = minimum::config();
            this->applyLayout();
            if (!cfg.showStats) return;

            auto& counters = minimum::counters();
            auto& stats = minimum::frameStats();
            uint32_t fps = counters.fps.load();
            bool throttled = counters.throttled.load();

            // The FPS number is measured with the wall clock inside the drawScene hook,
            // not derived from the scheduler, so it is the real number of frames drawn.
            char text[220];
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
#ifdef GEODE_IS_ANDROID
                double cpuMs = stats.averageCpuMs(60);
                std::snprintf(
                    text, sizeof(text),
                    "%s%u FPS%s | low %u | %.1f ms | cpu %.1f ms | logic %u/s | spikes %llu%s",
                    cfg.enabled ? "" : "(OFF) ", fps, throttled ? " (background)" : "",
                    low, frameMs, cpuMs, counters.logicFps.load(),
                    static_cast<unsigned long long>(counters.spikes.load()), sync
                );
#else
                std::snprintf(
                    text, sizeof(text),
                    "%s%u FPS%s | low %u | %.1f ms | logic %u/s | spikes %llu%s",
                    cfg.enabled ? "" : "(OFF) ", fps, throttled ? " (background)" : "",
                    low, frameMs, counters.logicFps.load(),
                    static_cast<unsigned long long>(counters.spikes.load()), sync
                );
#endif
            }
            else {
                std::snprintf(
                    text, sizeof(text), "%s%u FPS%s",
                    cfg.enabled ? "" : "(OFF) ", fps, throttled ? " (background)" : ""
                );
            }

            // Green when it holds the target, yellow when it does not quite, red when bad.
            switch (minimum::classifyFps(fps, cfg.targetFps)) {
                case minimum::FpsBand::Good: m_label->setColor(ccc3(120, 255, 120)); break;
                case minimum::FpsBand::Okay: m_label->setColor(ccc3(255, 225, 90)); break;
                case minimum::FpsBand::Bad: m_label->setColor(ccc3(255, 100, 100)); break;
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
        static clock::time_point s_fpsStart = clock::now();
        static minimum::LatencyGate s_latencyGate;
        static bool s_syncOk = false;

        auto const now = clock::now();
        double const rawDelta = std::chrono::duration<double>(now - s_last).count();
        s_last = now;
        // After a hitch (alt-tab, level load) do not try to "catch up" with a burst of draws.
        double const wallDelta = std::min(rawDelta, 0.25);

        minimum::refreshConfigIfDue(now);
        auto const& cfg = minimum::config();
        auto& counters = minimum::counters();

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
                counters.logicFps.store(static_cast<uint32_t>(s_fpsAll / span + 0.5));

                // Low latency mode only runs while the game is holding its frame rate, and
                // backs off for a growing amount of time when it costs frames (see
                // minimum::LatencyGate). It is not allowed to flip on and off every window.
                bool const wantSync = cfg.enabled && cfg.lowLatency && focused && totalFrames >= 300;
                double const nowSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
                s_syncOk = s_latencyGate.update(fps, nowSeconds, wantSync);

                s_fpsDrawn = 0;
                s_fpsAll = 0;
                s_fpsStart = now;
            }
        }

        // ---- Adaptive particle cap window (once per second) -----------------------
        if (measurable) {
            ++s_windowFrames;
            if (cfg.adaptiveCap && rawDelta * 1000.0 > 1500.0 / cfg.targetFps) {
                ++s_windowSlow;
            }
        }
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
    }
};
