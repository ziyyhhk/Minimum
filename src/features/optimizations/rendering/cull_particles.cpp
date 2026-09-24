#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCParticleSystem.hpp>

static bool minPerfOn() {
    return Mod::get()->getSettingValue<bool>("performance-mode");
}

static bool minCullOn() {
    return Mod::get()->getSettingValue<bool>("cull-particles") || minPerfOn();
}

static bool minLimitOn() {
    return Mod::get()->getSettingValue<bool>("limit-particles") || minPerfOn();
}

static bool minSkipDrawOn() {
    return Mod::get()->getSettingValue<bool>("skip-particle-draw") || minPerfOn();
}

static int minParticleCap() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("particle-cap"));
}

// Strong particle optimization for dense levels, OBS and Discord share.
// Culls offscreen systems, caps particle count, skips draw when off screen.
struct CullParticles : Modify<CullParticles, CCParticleSystem> {
    struct Fields {
        bool m_minOffscreen = false;
    };

    void update(float dt) {
        if (!minCullOn() && !minLimitOn()) {
            CCParticleSystem::update(dt);
            return;
        }

        if (!m_bIsActive) {
            CCParticleSystem::update(dt);
            return;
        }

        if (minLimitOn()) {
            int cap = minParticleCap();
            if (cap > 0 && static_cast<int>(m_uTotalParticles) > cap) {
                m_uTotalParticles = static_cast<unsigned int>(cap);
            }
        }

        if (minCullOn()) {
            auto world = this->convertToWorldSpace(CCPointZero);
            auto win = CCDirector::get()->getWinSize();

            // Larger margin in performance mode so more systems pause early
            float margin = minPerfOn() ? 250.f : 400.f;
            bool offscreen =
                world.x < -margin ||
                world.y < -margin ||
                world.x > win.width + margin ||
                world.y > win.height + margin;

            m_fields->m_minOffscreen = offscreen;

            // Fully skip simulation when far off screen and no live particles
            if (offscreen && m_uParticleCount == 0)
                return;

            // In performance mode, also skip update when offscreen even with a few particles left
            if (minPerfOn() && offscreen && m_uParticleCount < 8)
                return;
        }

        CCParticleSystem::update(dt);
    }

    void draw() {
        if (minSkipDrawOn() && m_fields->m_minOffscreen) {
            return;
        }
        if (minSkipDrawOn() && m_uParticleCount == 0) {
            return;
        }
        CCParticleSystem::draw();
    }
};
