#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <minimum.hpp>

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
        uint64_t m_lastDrawn = 0;
        uint64_t m_lastLogicOnly = 0;
        uint64_t m_lastIdleSkipped = 0;
        uint64_t m_lastSpikes = 0;
        double m_lastWorst = 0.0;
        std::string m_lastText;

        bool init() override {
            if (!CCNode::init()) return false;

            m_label = CCLabelBMFont::create("Minimum", "chatFont.fnt");
            if (!m_label) return false;
            this->addChild(m_label);

            this->schedule(schedule_selector(StatsHud::tick), 0.5f);
            tick(0.f);
            return true;
        }

        void layout() {
            auto const& cfg = minimum::config();
            auto winSize = CCDirector::get()->getWinSize();
            bool right = cfg.hudCorner == minimum::HudCorner::TopRight || cfg.hudCorner == minimum::HudCorner::BottomRight;
            bool bottom = cfg.hudCorner == minimum::HudCorner::BottomLeft || cfg.hudCorner == minimum::HudCorner::BottomRight;

            m_label->setAnchorPoint({right ? 1.f : 0.f, bottom ? 0.f : 1.f});
            m_label->setPosition({
                right ? winSize.width - kHudMargin : kHudMargin,
                bottom ? kHudMargin : winSize.height - kHudMargin
            });
            m_label->setScale(cfg.hudScale);
            m_label->setOpacity(cfg.hudOpacity);
        }

        void tick(float) {
            auto const& cfg = minimum::config();
            m_label->setVisible(cfg.showStats);
            if (!cfg.showStats) return;

            layout();

            auto& counters = minimum::counters();
            uint64_t drawn = counters.framesDrawn.load();
            uint64_t logicOnly = counters.framesLogicOnly.load();
            uint64_t idleSkipped = counters.idleParticleDrawsSkipped.load();
            uint64_t spikes = counters.frameSpikes.load();
            double worst = counters.worstFrameMs.load();

            char text[280];
            if (cfg.hudDetailed) {
                std::snprintf(
                    text, sizeof(text),
                    "Min %s | %llu fps | logic %llu | idle skip %llu | spike %llu | worst %.1f ms",
                    cfg.enabled ? "ON" : "OFF",
                    static_cast<unsigned long long>(drawn - m_lastDrawn),
                    static_cast<unsigned long long>(logicOnly - m_lastLogicOnly),
                    static_cast<unsigned long long>(idleSkipped - m_lastIdleSkipped),
                    static_cast<unsigned long long>(spikes - m_lastSpikes),
                    worst
                );
            } else {
                std::snprintf(
                    text, sizeof(text),
                    "Min %s | %llu fps | worst %.0f ms | spikes %llu",
                    cfg.enabled ? "ON" : "OFF",
                    static_cast<unsigned long long>(drawn - m_lastDrawn),
                    worst,
                    static_cast<unsigned long long>(spikes - m_lastSpikes)
                );
            }

            if (text != m_lastText) {
                m_label->setString(text);
                m_lastText = text;
            }

            m_lastDrawn = drawn;
            m_lastLogicOnly = logicOnly;
            m_lastIdleSkipped = idleSkipped;
            m_lastSpikes = spikes;
            m_lastWorst = worst;
            counters.worstFrameMs.store(0.0);
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
        static bool s_wasFocused = true;

        auto const now = clock::now();
        double const rawDelta = std::chrono::duration<double>(now - s_last).count();
        s_last = now;
        double wallDelta = std::min(rawDelta, 0.25);

        minimum::refreshConfigIfDue(now);
        auto const& cfg = minimum::config();
        auto& counters = minimum::counters();

        unsigned int totalFrames = this->getTotalFrames();
        bool const focused = minimum::processHasForeground(now);

        // ---- Focus changes (Windows): tab-out volume ------------------------------
        if (focused != s_wasFocused) {
            s_wasFocused = focused;
            minimum::onFocusChanged(focused);
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
            double ms = rawDelta * 1000.0;
            if (ms > counters.worstFrameMs.load()) {
                counters.worstFrameMs.store(ms);
            }
            if (cfg.spikeLogger && ms >= cfg.spikeThresholdMs) {
                ++counters.frameSpikes;
                using namespace std::chrono_literals;
                if (now - s_lastSpikeLog >= 200ms) {
                    s_lastSpikeLog = now;
                    uint64_t created = counters.particleSystemsCreated.load() - s_createdSeen;
                    float playerX = 0.f;
                    if (auto* pl = PlayLayer::get()) {
                        if (auto* player = pl->m_player1) {
                            playerX = player->getPositionX();
                        }
                    }
                    log::warn(
                        "Frame spike: {:.1f} ms at player x={:.0f}, particle systems created this frame={}",
                        ms, playerX, created
                    );
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
        (void)wallDelta;
        (void)s_accum;
#endif

        CCDirector::drawScene();
        ++counters.framesDrawn;

        ensureHud(totalFrames);
    }
};
