#include <Geode/Geode.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <algorithm>
#include <cmath>
#include <minimum.hpp>
#include <minimum_logic.hpp>

using namespace geode::prelude;

// Particles
//
// 1. Skip idle draws. CCParticleSystemQuad::draw sets up the shader, binds the
//    texture and the buffers and issues a draw call even when the system has no
//    live particles. A level has dozens of such systems (player effects, ground
//    particles, trails) that are idle most of the time. Skipping the draw when
//    m_uParticleCount == 0 changes nothing visually.
//
// 2. Cap the pool size when a system is created. Levels can create systems with
//    thousands of particles. Clamping the requested total BEFORE the original
//    function runs means the particle, quad and index buffers are all allocated
//    for the capped size, so nothing can ever index past its buffer. (The old
//    version of this mod patched m_uTotalParticles on live systems every frame.)
//    The cap applies to systems created after the setting changes, so re-enter
//    the level after changing it.
//
// 3. Adaptive cap (opt-in). If the game keeps missing the target FPS, the cap is
//    lowered in steps (cap, cap/2, cap/4, cap/8, never below 16) and raised again
//    after the game has been smooth for a while. The step decisions live in
//    minimum_logic.hpp (AdaptiveCapGovernor) and are unit tested. Only affects
//    systems created afterwards. Visual only.
//
// 4. Thermal response (Android, opt-in with the adaptive cap). While the phone reports
//    SEVERE or worse, the cap drops one step at once and then further steps are
//    taken at most every 5 s. It recovers after 30 s of nominal temperature. The rules
//    are in ThermalGovernor (minimum_logic.hpp). The status is polled once a second
//    (see pollThermal in perf_hint.cpp) and not taken from a listener, because a
//    listener would run on a system thread.
//
// 5. Spawn budget (opt-in). Only during a level: after `spawnBudget` particle systems have
//    been created in one frame, further systems still exist but get a pool of 4.
//
// 6. Off-screen culling (opt-in). A relative-position system whose reach (see
//    particleReach) does not touch the visible rectangle is not drawn. Its update still
//    runs, so emission and motion keep their timing and look the same when it comes back
//    on screen. Free and grouped systems are always drawn, because their particles are not
//    placed relative to the emitter and no safe bound can be computed for them.
//
// Nothing here touches gameplay, timing or physics.

namespace minimum {

    namespace {
        AdaptiveCapGovernor g_adaptive;
        ThermalGovernor g_thermal;
        // particleSystemsCreated when the current frame hook call started (see particleFrameBegin).
        uint64_t g_frameCreatedBase = 0;
    }

    void particleFrameBegin() {
        g_frameCreatedBase = counters().particleSystemsCreated.load(std::memory_order_relaxed);
    }

    int currentCapStep() {
        return combinedCapStep(g_adaptive.step(), g_thermal.step());
    }

    int thermalCapStep() {
        return g_thermal.step();
    }

    unsigned int effectiveParticleCap() {
        return cappedPoolSize(config().particleCap, currentCapStep());
    }

    void noteFrameWindow(uint32_t frames, uint32_t slowFrames) {
        auto const& cfg = config();
        if (!cfg.enabled || !cfg.capParticles || !cfg.adaptiveCap) {
            g_adaptive.reset();
            return;
        }
        g_adaptive.noteWindow(frames, slowFrames);
    }

    void noteThermalStatus(int status, double nowSec) {
        auto const& cfg = config();
        if (!cfg.enabled || !cfg.capParticles || !cfg.adaptiveCap) {
            g_thermal.reset();
            return;
        }
        g_thermal.update(status, nowSec);
    }

}

namespace {

    // Particle systems created earlier in this frame hook call (before the one being created).
    uint64_t createdEarlierThisFrame() {
        uint64_t const total = minimum::counters().particleSystemsCreated.load(std::memory_order_relaxed);
        return total >= minimum::g_frameCreatedBase ? total - minimum::g_frameCreatedBase : 0;
    }

    // The pool size a system really gets: the spawn budget first, then the cap. Counts every
    // system whose pool was made smaller.
    unsigned int limitedPool(unsigned int requested, uint64_t createdEarlier) {
        auto const& cfg = minimum::config();
        if (!cfg.enabled) return requested;

        unsigned int pool = requested;
        if (cfg.spawnBudget > 0 && PlayLayer::get() != nullptr) {
            pool = minimum::budgetedPoolSize(pool, createdEarlier, cfg.spawnBudget);
        }
        if (cfg.capParticles) {
            pool = std::min(pool, minimum::effectiveParticleCap());
        }
        if (pool < requested) ++minimum::counters().particlePoolsCapped;
        return pool;
    }

    // True when the system is a relative-position system whose reach does not touch the
    // visible rectangle. Every number comes from the cocos2d header of this game version
    // (loader/include/Geode/cocos/particle_nodes/CCParticleSystem.h). Degenerate transforms and
    // NaN values answer "not off screen", so the system is drawn.
    bool isFarOffScreen(CCParticleSystemQuad* system) {
        if (system->getPositionType() != kCCPositionTypeRelative) return false;

        // World space scale of the emitter: how far one local unit moves on screen.
        CCPoint const origin = system->convertToWorldSpace(ccp(0.f, 0.f));
        CCPoint const unitX = system->convertToWorldSpace(ccp(1.f, 0.f));
        CCPoint const unitY = system->convertToWorldSpace(ccp(0.f, 1.f));
        double const scale = std::max(
            std::hypot(unitX.x - origin.x, unitX.y - origin.y),
            std::hypot(unitY.x - origin.x, unitY.y - origin.y)
        );
        if (!std::isfinite(scale) || scale <= 0.0) return false;

        CCPoint const& gravity = system->getGravity();
        CCPoint const& source = system->getSourcePosition();
        CCPoint const& posVar = system->getPosVar();

        minimum::ParticleBounds bounds;
        bounds.speed = std::fabs(system->getSpeed()) + std::fabs(system->getSpeedVar());
        bounds.life = std::fabs(system->getLife()) + std::fabs(system->getLifeVar());
        bounds.accel = std::hypot(gravity.x, gravity.y)
                     + std::fabs(system->getRadialAccel()) + std::fabs(system->getRadialAccelVar())
                     + std::fabs(system->getTangentialAccel()) + std::fabs(system->getTangentialAccelVar());
        bounds.radius = std::max(
            std::fabs(system->getStartRadius()) + std::fabs(system->getStartRadiusVar()),
            std::fabs(system->getEndRadius()) + std::fabs(system->getEndRadiusVar())
        );
        bounds.posVar = std::hypot(posVar.x, posVar.y);
        bounds.sourceLength = std::hypot(source.x, source.y);
        bounds.maxSize = std::max(
            std::fabs(system->getStartSize()) + std::fabs(system->getStartSizeVar()),
            std::fabs(system->getEndSize()) + std::fabs(system->getEndSizeVar())
        );

        float const reach = minimum::particleReach(bounds) * static_cast<float>(scale);
        if (!std::isfinite(reach)) return false;

        CCSize const size = CCDirector::get()->getVisibleSize();
        CCPoint const visibleOrigin = CCDirector::get()->getVisibleOrigin();
        minimum::Box const view{visibleOrigin.x, visibleOrigin.y, size.width, size.height};
        minimum::Vec2 const center{origin.x, origin.y};
        return !minimum::squareTouches(center, reach, view);
    }

}

struct ParticleOpt : Modify<ParticleOpt, CCParticleSystemQuad> {
    bool initWithTotalParticles(unsigned int total, bool flag) {
        // Counted so the spike logger can tell "a frame was slow because it created N particle systems".
        uint64_t const earlier = createdEarlierThisFrame();
        ++minimum::counters().particleSystemsCreated;
        return CCParticleSystemQuad::initWithTotalParticles(limitedPool(total, earlier), flag);
    }

    void setTotalParticles(unsigned int total) {
        CCParticleSystemQuad::setTotalParticles(limitedPool(total, createdEarlierThisFrame()));
    }

    void draw() {
        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.skipIdleParticles && m_uParticleCount == 0) {
            ++minimum::counters().idleParticleDrawsSkipped;
            return;
        }
        if (cfg.enabled && cfg.cullOffscreenParticles && m_uParticleCount > 0 && isFarOffScreen(this)) {
            ++minimum::counters().offscreenParticleDrawsSkipped;
            return;
        }
        CCParticleSystemQuad::draw();
    }
};
