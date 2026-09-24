#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCDirector.hpp>

// Inspired by mat.draw-divide: render at display refresh rate while logic runs at full FPS.
// Helps GPU bound systems when using high FPS bypass.

static double g_deltaAccum = 0.0;

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minDrawDivide() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("draw-divide") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

static double minVisualFps() {
    double fps = Mod::get()->getSettingValue<double>("visual-fps");
    if (fps < 30.0) fps = 30.0;
    if (fps > 360.0) fps = 360.0;
    return fps;
}

struct DrawDivide : Modify<DrawDivide, CCDirector> {
    void drawScene() {
        if (!minDrawDivide() || this->getTotalFrames() < 60) {
            CCDirector::drawScene();
            return;
        }

        const double targetDelta = 1.0 / minVisualFps();
        g_deltaAccum += this->getActualDeltaTime();

        if (g_deltaAccum >= targetDelta) {
            g_deltaAccum -= targetDelta;
            // Avoid huge spiral if a long hitch happened
            if (g_deltaAccum > targetDelta * 2.0)
                g_deltaAccum = 0.0;
            CCDirector::drawScene();
            return;
        }

        // Logic only: update scheduler, skip full GL clear/visit
        if (!this->isPaused()) {
            this->getScheduler()->update(this->getDeltaTime());
        }
        if (this->getNextScene()) {
            this->setNextScene();
        }
    }
};
