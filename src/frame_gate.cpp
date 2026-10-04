#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <minimum.hpp>

using namespace geode::prelude;

// Frame gate
//
// Hooks CCDirector::drawScene and decides, per frame, whether the scene is
// drawn or only updated ("logic-only" frame). Used for:
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

    class StatsHud : public CCNode {
    protected:
        CCLabelBMFont* m_label = nullptr;
        uint64_t m_lastDrawn = 0;
        uint64_t m_lastLogicOnly = 0;
        uint64_t m_lastIdleSkipped = 0;

        bool init() override {
            if (!CCNode::init()) return false;

            m_label = CCLabelBMFont::create("Minimum", "chatFont.fnt");
            if (!m_label) return false;
            m_label->setAnchorPoint({0.f, 1.f});
            m_label->setScale(0.5f);
            m_label->setOpacity(200);
            auto winSize = CCDirector::get()->getWinSize();
            m_label->setPosition({4.f, winSize.height - 4.f});
            this->addChild(m_label);

            this->schedule(schedule_selector(StatsHud::tick), 1.f);
            return true;
        }

        void tick(float) {
            auto const& cfg = minimum::config();
            m_label->setVisible(cfg.showStats);
            if (!cfg.showStats) return;

            auto& counters = minimum::counters();
            uint64_t drawn = counters.framesDrawn.load();
            uint64_t logicOnly = counters.framesLogicOnly.load();
            uint64_t idleSkipped = counters.idleParticleDrawsSkipped.load();
            uint64_t capped = counters.particlePoolsCapped.load();

            char text[220];
            std::snprintf(
                text, sizeof(text),
                "Minimum %s | drawn %llu/s, logic-only %llu/s | idle particle draws skipped %llu/s | particle pools capped %llu",
                cfg.enabled ? "ON" : "OFF",
                static_cast<unsigned long long>(drawn - m_lastDrawn),
                static_cast<unsigned long long>(logicOnly - m_lastLogicOnly),
                static_cast<unsigned long long>(idleSkipped - m_lastIdleSkipped),
                static_cast<unsigned long long>(capped)
            );
            m_label->setString(text);

            m_lastDrawn = drawn;
            m_lastLogicOnly = logicOnly;
            m_lastIdleSkipped = idleSkipped;
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
        static double s_accum = 0.0;

        auto const now = clock::now();
        double wallDelta = std::chrono::duration<double>(now - s_last).count();
        s_last = now;
        // After a hitch (alt-tab, level load) do not try to "catch up" with a burst of draws.
        wallDelta = std::min(wallDelta, 0.25);

        minimum::refreshConfigIfDue(now);
        auto const& cfg = minimum::config();
        auto& counters = minimum::counters();

        // Seconds between two drawn frames. 0 means: draw every frame.
        double interval = 0.0;
        if (cfg.enabled) {
            if (cfg.backgroundThrottle && !minimum::processHasForeground(now)) {
                interval = 1.0 / cfg.backgroundFps;
            }
            else if (cfg.drawDivide) {
                interval = 1.0 / cfg.visualFps;
            }
        }

        unsigned int totalFrames = this->getTotalFrames();
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

        CCDirector::drawScene();
        ++counters.framesDrawn;

        ensureHud(totalFrames);
    }
};
