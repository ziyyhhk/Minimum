#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCParticleSystem.hpp>

// Pause particle systems that are far off screen so they stop simulating.
// Helps a lot on levels that spam particles outside the play area.
struct CullParticles : Modify<CullParticles, CCParticleSystem> {
    void update(float dt) {
        if (!Mod::get()->getSettingValue<bool>("cull-particles")) {
            CCParticleSystem::update(dt);
            return;
        }

        if (!m_bIsActive) {
            CCParticleSystem::update(dt);
            return;
        }

        auto world = this->convertToWorldSpace(CCPointZero);
        auto win = CCDirector::get()->getWinSize();

        constexpr float margin = 400.f;
        bool offscreen =
            world.x < -margin ||
            world.y < -margin ||
            world.x > win.width + margin ||
            world.y > win.height + margin;

        if (offscreen && m_uParticleCount == 0)
            return;

        CCParticleSystem::update(dt);
    }
};
