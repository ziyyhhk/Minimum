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
// Hooks CCDirector::drawScene. On every platform it measures frame times (spike
// logger, stats line, adaptive particle cap) and then draws the frame normally.
// On Windows only, it additionally decides per frame whether the scene is drawn
// or only updated ("logic-only" frame). Used for:
//
//   * Background throttle: while the game window is not the foreground
//     window, draw at a low rate (default 20 fps). Nobody is looking, so the
//     GPU and the CPU side of rendering are freed up.
//   * Draw Divide (opt-in): draw at "Visual FPS" while game logic keeps
//     running at the full frame rate. Same idea as the mat.draw-divide mod.
//
// A logic-only frame runs exactly what the proven Draw Divide implementation
// runs: scheduler update with the delta time the game's main loop already
// computed, plus a pending scene switch. calculateDeltaTime() is NOT called
// on those frames (calling it there creates a speed hack).
//
// Timing uses std::chrono::steady_clock and not the game's own delta time,
// so it does not depend on how the game's main loop is paced.
//
// The first 300 frames (loading) and frames with a pending scene switch are
// always drawn normally.
//
// Spike logger: the gap between two drawScene calls is the real frame time as
// the player feels it (it includes vsync waits, GC-like allocation stalls,
// saves, anything). When a gap is longer than the threshold it is written to the
// Geode log together with where in the level it happened and how many particle
// systems were created during that frame, so a lag spike can be traced to a cause.
//
// The stats line is deliberately short and updated once per second. A long
// CCLabelBMFont string rebuilds one sprite per character on every setString,
// which is itself a small periodic hitch.

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
            uint32_t fps = counters.fps.load();
            bool throttled = counters.throttled.load();

            // The FPS number is measured with the wall clock inside the drawScene hook,
            // not derived from the scheduler, so it is the real number of frames drawn.
            char text[160];
            if (cfg.hudDetailed) {
                uint64_t idleSkipped = counters.idleParticleDrawsSkipped.load();
                double worstMs = counters.worstFrameUs.exchange(0) / 1000.0;
                std::snprintf(
                    text, sizeof(text),
                    "%s%u FPS%s | logic %u/s | worst %.1f ms | spikes %llu | idle skipped %llu | capped %llu",
                    cfg.enabled ? "" : "(OFF) ",
                    fps,
                    throttled ? " (background)" : "",
                    counters.logicFps.load(),
                    worstMs,
                    static_cast<unsigned long long>(counters.spikes.load()),
                    static_cast<unsigned long long>(idleSkipped),
                    static_cast<unsigned long long>(counters.particlePoolsCapped.load())
                );
            }
            else {
                std::snprintf(
                    text, sizeof(text),
                    "%s%u FPS%s",
                    cfg.enabled ? "" : "(OFF) ",
                    fps,
                    throttled ? " (background)" : ""
                );
            }

            // Green when smooth, yellow when so-so, red when bad.
            if (fps >= 55) m_label->setColor(ccc3(120, 255, 120));
            else if (fps >= 30) m_label->setColor(ccc3(255, 225, 90));
            else m_label->setColor(ccc3(255, 100, 100));

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

    void ensureHud(unsigned int totalFrames) {
        if (g_hudCreated || totalFrames < 200) return;
        if (!minimum::config().showStats) return;

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
        if (totalFrames >= 300 && focused) {
            ++s_windowFrames;
            if (cfg.adaptiveCap && rawDelta * 1000.0 > 1500.0 / cfg.adaptiveTargetFps) {
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
        // Skipped while loading (first 300 frames) and while unfocused, where long gaps are expected.
        if (totalFrames >= 300 && focused) {
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

        CCDirector::drawScene();
        ++counters.framesDrawn;
        ++s_fpsDrawn;

        // Low latency mode: let the GPU catch up before the next frame starts, so
        // input is not stuck behind frames queued in the driver. Works on desktop
        // and on mobile (GLES exports glFinish under the same name).
        if (cfg.enabled && cfg.lowLatency && s_syncOk && focused && totalFrames >= 300) {
            minimum::hardGpuSync();
        }

        ensureHud(totalFrames);
    }
};
