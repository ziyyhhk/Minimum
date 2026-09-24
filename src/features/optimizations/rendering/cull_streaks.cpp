#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCMotionStreak.hpp>

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minCullStreaks() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("cull-streaks") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

// Skip updating motion streaks that are far off screen.
struct CullStreaks : Modify<CullStreaks, CCMotionStreak> {
    struct Fields {
        bool m_off = false;
    };

    void update(float dt) {
        if (!minCullStreaks()) {
            CCMotionStreak::update(dt);
            return;
        }

        auto world = this->convertToWorldSpace(CCPointZero);
        auto win = CCDirector::get()->getWinSize();
        float margin = 150.f;
        bool off =
            world.x < -margin || world.y < -margin ||
            world.x > win.width + margin || world.y > win.height + margin;

        m_fields->m_off = off;
        if (off)
            return;

        CCMotionStreak::update(dt);
    }

    void draw() {
        if (minCullStreaks() && m_fields->m_off)
            return;
        CCMotionStreak::draw();
    }
};
