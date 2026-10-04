#include <Geode/Geode.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <minimum.hpp>

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

namespace {

    unsigned int cappedTotal(unsigned int requested) {
        auto const& cfg = minimum::config();
        if (!cfg.enabled || !cfg.capParticles || requested <= cfg.particleCap) {
            return requested;
        }
        ++minimum::counters().particlePoolsCapped;
        return cfg.particleCap;
    }

}

struct ParticleOpt : Modify<ParticleOpt, CCParticleSystemQuad> {
    bool initWithTotalParticles(unsigned int total, bool flag) {
        return CCParticleSystemQuad::initWithTotalParticles(cappedTotal(total), flag);
    }

    void setTotalParticles(unsigned int total) {
        CCParticleSystemQuad::setTotalParticles(cappedTotal(total));
    }

    void draw() {
        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.skipIdleParticles && m_uParticleCount == 0) {
            ++minimum::counters().idleParticleDrawsSkipped;
            return;
        }
        CCParticleSystemQuad::draw();
    }
};
