#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCDirector.hpp>

// Optional. Default OFF. Not forced by Performance Mode anymore.
// Only skips full scene draws; logic still runs. GPU can get better FPS
// while the in-game FPS counter still shows logic rate (TPS).

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
        if (!minDrawDivide() || this->getTotalFrames() < 120) {
            CCDirector::drawScene();
            return;
        }

        const double targetDelta = 1.0 / minVisualFps();
        g_deltaAccum += this->getActualDeltaTime();

        if (g_deltaAccum >= targetDelta) {
            g_deltaAccum -= targetDelta;
            if (g_deltaAccum > targetDelta * 2.0)
                g_deltaAccum = 0.0;
            CCDirector::drawScene();
            return;
        }

        if (!this->isPaused()) {
            this->getScheduler()->update(this->getDeltaTime());
        }
        if (this->getNextScene()) {
            this->setNextScene();
        }
    }
};
