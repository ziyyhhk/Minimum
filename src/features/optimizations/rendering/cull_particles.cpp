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

        // If the system is not active, nothing to do
        if (!m_bIsActive) {
            CCParticleSystem::update(dt);
            return;
        }

        // World position of the emitter
        auto world = this->convertToWorldSpace(CCPointZero);
        auto* win = CCDirector::get()->getWinSize();

        // Generous margin so particles near the edge still update
        constexpr float margin = 400.f;
        bool offscreen =
            world.x < -margin ||
            world.y < -margin ||
            world.x > win.width + margin ||
            world.y > win.height + margin;

        if (offscreen) {
            // Still advance time a little so lifetime expires, but skip heavy work
            // by not calling the full update when there are no live particles
            if (m_uParticleCount == 0)
                return;
        }

        CCParticleSystem::update(dt);
    }
};
