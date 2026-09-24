#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCParticleSystem.hpp>

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minPerf() {
    return minOn() && Mod::get()->getSettingValue<bool>("performance-mode");
}

static bool minCull() {
    return minOn() && (Mod::get()->getSettingValue<bool>("cull-particles") || minPerf());
}

static bool minLimit() {
    return minOn() && (Mod::get()->getSettingValue<bool>("limit-particles") || minPerf());
}

static bool minReduceEmit() {
    return minOn() && (Mod::get()->getSettingValue<bool>("reduce-emission") || minPerf());
}

static bool minSkipDraw() {
    return minOn() && (Mod::get()->getSettingValue<bool>("skip-particle-draw") || minPerf());
}

static unsigned int minCap() {
    int c = static_cast<int>(Mod::get()->getSettingValue<int64_t>("particle-cap"));
    if (c < 8) c = 8;
    return static_cast<unsigned int>(c);
}

static float minEmitScale() {
    float s = static_cast<float>(Mod::get()->getSettingValue<double>("emission-scale"));
    if (s < 0.1f) s = 0.1f;
    if (s > 1.f) s = 1.f;
    return s;
}

// Safer particle optimization. No stopSystem/resetSystem (those caused bugs).
// Caps, emission scale, skip update/draw offscreen. Live toggles.
struct CullParticles : Modify<CullParticles, CCParticleSystem> {
    struct Fields {
        bool m_offscreen = false;
        float m_baseEmission = -1.f;
        bool m_emissionScaled = false;
    };

    void update(float dt) {
        if (!minOn()) {
            if (m_fields->m_emissionScaled && m_fields->m_baseEmission >= 0.f) {
                m_fEmissionRate = m_fields->m_baseEmission;
                m_fields->m_emissionScaled = false;
            }
            CCParticleSystem::update(dt);
            return;
        }

        if (!m_bIsActive) {
            CCParticleSystem::update(dt);
            return;
        }

        // Cap total and live particles every frame so the game cannot grow past the limit
        if (minLimit()) {
            auto cap = minCap();
            if (m_uTotalParticles > cap)
                m_uTotalParticles = cap;
            if (m_uParticleCount > cap)
                m_uParticleCount = cap;
        }

        // Scale emission rate (remember original once)
        if (minReduceEmit()) {
            if (!m_fields->m_emissionScaled) {
                m_fields->m_baseEmission = m_fEmissionRate;
                m_fields->m_emissionScaled = true;
            }
            m_fEmissionRate = m_fields->m_baseEmission * minEmitScale();
        } else if (m_fields->m_emissionScaled) {
            m_fEmissionRate = m_fields->m_baseEmission;
            m_fields->m_emissionScaled = false;
        }

        if (minCull()) {
            auto world = this->convertToWorldSpace(CCPointZero);
            auto win = CCDirector::get()->getWinSize();
            float margin = minPerf() ? 80.f : 200.f;

            bool off =
                world.x < -margin ||
                world.y < -margin ||
                world.x > win.width + margin ||
                world.y > win.height + margin;

            m_fields->m_offscreen = off;

            // Skip full simulation when offscreen and almost empty
            if (off && m_uParticleCount <= 4)
                return;
        } else {
            m_fields->m_offscreen = false;
        }

        CCParticleSystem::update(dt);
    }

    void draw() {
        if (!minOn()) {
            CCParticleSystem::draw();
            return;
        }
        if (minSkipDraw() && (m_fields->m_offscreen || m_uParticleCount == 0))
            return;
        CCParticleSystem::draw();
    }
};

$on_mod(Loaded) {
    log::info("Minimum v1.4.0 loaded. Use Enable Minimum to A/B test FPS live.");
}
