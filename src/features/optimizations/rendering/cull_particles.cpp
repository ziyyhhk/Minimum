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

static int minCap() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("particle-cap"));
}

static float minEmitScale() {
    return static_cast<float>(Mod::get()->getSettingValue<double>("emission-scale"));
}

// Aggressive particle optimization. All checks are live (no restart).
// Designed for dense levels (Amethyst style) and recording / Discord share.
struct CullParticles : Modify<CullParticles, CCParticleSystem> {
    struct Fields {
        bool m_offscreen = false;
        float m_origEmission = -1.f;
        unsigned int m_origTotal = 0;
        bool m_stopped = false;
    };

    void update(float dt) {
        if (!minOn()) {
            // Restore emission if we had scaled it
            if (m_fields->m_origEmission > 0.f) {
                m_fEmissionRate = m_fields->m_origEmission;
                m_fields->m_origEmission = -1.f;
            }
            CCParticleSystem::update(dt);
            return;
        }

        if (!m_bIsActive) {
            CCParticleSystem::update(dt);
            return;
        }

        // Hard particle cap
        if (minLimit()) {
            int cap = minCap();
            if (cap > 0) {
                if (m_fields->m_origTotal == 0)
                    m_fields->m_origTotal = m_uTotalParticles;
                if (m_uTotalParticles > static_cast<unsigned int>(cap))
                    m_uTotalParticles = static_cast<unsigned int>(cap);
                // Also kill excess live particles
                if (m_uParticleCount > static_cast<unsigned int>(cap))
                    m_uParticleCount = static_cast<unsigned int>(cap);
            }
        }

        // Reduce emission rate (causes fewer lag spikes from bursts)
        if (minReduceEmit()) {
            if (m_fields->m_origEmission < 0.f)
                m_fields->m_origEmission = m_fEmissionRate;
            float scale = minEmitScale();
            if (scale < 0.1f) scale = 0.1f;
            if (scale > 1.f) scale = 1.f;
            m_fEmissionRate = m_fields->m_origEmission * scale;
        } else if (m_fields->m_origEmission > 0.f) {
            m_fEmissionRate = m_fields->m_origEmission;
            m_fields->m_origEmission = -1.f;
        }

        if (minCull()) {
            // World position of the system
            auto world = this->convertToWorldSpace(CCPointZero);
            auto win = CCDirector::get()->getWinSize();

            // Tighter margin in performance mode
            float margin = minPerf() ? 120.f : 280.f;
            bool off =
                world.x < -margin ||
                world.y < -margin ||
                world.x > win.width + margin ||
                world.y > win.height + margin;

            m_fields->m_offscreen = off;

            if (off) {
                // Fully stop simulation when far away
                if (m_uParticleCount == 0 || (minPerf() && m_uParticleCount < 12)) {
                    if (!m_fields->m_stopped) {
                        this->stopSystem();
                        m_fields->m_stopped = true;
                    }
                    return;
                }
            } else {
                // Came back on screen: allow emit again
                if (m_fields->m_stopped) {
                    this->resetSystem();
                    m_fields->m_stopped = false;
                }
            }
        }

        CCParticleSystem::update(dt);
    }

    void draw() {
        if (!minOn()) {
            CCParticleSystem::draw();
            return;
        }
        if (minSkipDraw()) {
            if (m_fields->m_offscreen)
                return;
            if (m_uParticleCount == 0)
                return;
        }
        CCParticleSystem::draw();
    }
};

$on_mod(Loaded) {
    listenForSettingChanges("mod-enabled", [](bool v) {
        log::info("Minimum: mod {}", v ? "ENABLED" : "DISABLED");
    });
    listenForSettingChanges("performance-mode", [](bool v) {
        log::info("Minimum: performance mode {}", v ? "ON" : "OFF");
    });
    listenForSettingChanges("particle-cap", [](int64_t v) {
        log::info("Minimum: particle cap set to {}", v);
    });
}
