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
    if (c < 4) c = 4;
    return static_cast<unsigned int>(c);
}

static float minEmitScale() {
    float s = static_cast<float>(Mod::get()->getSettingValue<double>("emission-scale"));
    if (s < 0.05f) s = 0.05f;
    if (s > 1.f) s = 1.f;
    return s;
}

static float minCullMargin() {
    return static_cast<float>(Mod::get()->getSettingValue<double>("cull-margin"));
}

struct CullParticles : Modify<CullParticles, CCParticleSystem> {
    struct Fields {
        bool m_offscreen = false;
        float m_baseEmission = -1.f;
        bool m_scaled = false;
    };

    void update(float dt) {
        if (!minOn()) {
            if (m_fields->m_scaled && m_fields->m_baseEmission >= 0.f) {
                m_fEmissionRate = m_fields->m_baseEmission;
                m_fields->m_scaled = false;
            }
            CCParticleSystem::update(dt);
            return;
        }

        if (!m_bIsActive) {
            CCParticleSystem::update(dt);
            return;
        }

        if (minLimit()) {
            auto cap = minCap();
            if (m_uTotalParticles > cap) m_uTotalParticles = cap;
            if (m_uParticleCount > cap) m_uParticleCount = cap;
        }

        if (minReduceEmit()) {
            if (!m_fields->m_scaled) {
                m_fields->m_baseEmission = m_fEmissionRate;
                m_fields->m_scaled = true;
            }
            m_fEmissionRate = m_fields->m_baseEmission * minEmitScale();
        } else if (m_fields->m_scaled) {
            m_fEmissionRate = m_fields->m_baseEmission;
            m_fields->m_scaled = false;
        }

        if (minCull()) {
            auto world = this->convertToWorldSpace(CCPointZero);
            auto win = CCDirector::get()->getWinSize();
            float margin = minCullMargin();
            if (minPerf() && margin > 40.f) margin = 40.f;

            bool off =
                world.x < -margin || world.y < -margin ||
                world.x > win.width + margin || world.y > win.height + margin;

            m_fields->m_offscreen = off;
            if (off && m_uParticleCount <= 8)
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
