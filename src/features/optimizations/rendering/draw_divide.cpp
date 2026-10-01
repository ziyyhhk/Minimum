#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCDirector.hpp>

// Optional. Default OFF. Not forced by Performance Mode.
// Only skips full scene draws; logic still runs at full rate.
// GPU-bound players can gain FPS; CPU-bound may see little gain or worse.
// WARNING: Do not run together with mat.draw-divide (both hook drawScene).

static double g_deltaAccum = 0.0;

static bool minDrawDivide() {
    return Mod::get()->getSettingValue<bool>("mod-enabled")
        && Mod::get()->getSettingValue<bool>("draw-divide");
}

static double minVisualFps() {
    double fps = Mod::get()->getSettingValue<double>("visual-fps");
    if (fps < 30.0) fps = 30.0;
    if (fps > 360.0) fps = 360.0;
    return fps;
}

struct DrawDivide : Modify<DrawDivide, CCDirector> {
    void drawScene() {
        // Skip early frames so the game finishes loading cleanly
        if (!minDrawDivide() || this->getTotalFrames() < 150) {
            CCDirector::drawScene();
            return;
        }

        const double targetDelta = 1.0 / minVisualFps();
        g_deltaAccum += this->getActualDeltaTime();

        if (g_deltaAccum >= targetDelta) {
            g_deltaAccum -= targetDelta;
            // Prevent spiral of death if we fall far behind
            if (g_deltaAccum > targetDelta * 2.0)
                g_deltaAccum = 0.0;
            CCDirector::drawScene();
            return;
        }

        // Logic-only frame (no full draw / glClear / visit)
        if (!this->isPaused()) {
            this->getScheduler()->update(this->getDeltaTime());
        }
        if (this->getNextScene()) {
            this->setNextScene();
        }
    }
};
