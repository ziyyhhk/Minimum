#include <Geode/Geode.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <algorithm>
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

// 3. Adaptive cap (opt-in). If the game keeps missing the target FPS, the cap is
//    lowered in steps (cap, cap/2, cap/4, cap/8, never below 16) and raised again
//    after the game has been smooth for a while. Only affects systems created
//    afterwards, which in a level is constantly (orbs, pads, death effects, trails).
//    Visual only: nothing here touches gameplay, timing or physics.

namespace minimum {

    namespace {
        int g_step = 0;
        int g_badWindows = 0;
        int g_goodWindows = 0;
    }

    unsigned int effectiveParticleCap() {
        unsigned int base = config().particleCap;
        if (g_step == 0) return base;
        unsigned int lowered = std::max(base >> g_step, 16u);
        return std::min(base, lowered);
    }

    void noteFrameWindow(uint32_t frames, uint32_t slowFrames) {
        auto const& cfg = config();
        if (!cfg.enabled || !cfg.capParticles || !cfg.adaptiveCap) {
            g_step = 0;
            g_badWindows = 0;
            g_goodWindows = 0;
            return;
        }
        if (frames < 10) return;

        double slowRatio = static_cast<double>(slowFrames) / static_cast<double>(frames);
        if (slowRatio > 0.25) {
            g_goodWindows = 0;
            // Two bad seconds in a row before lowering, so one hitch does not change anything.
            if (++g_badWindows >= 2) {
                g_badWindows = 0;
                if (g_step < 3) ++g_step;
            }
        }
        else if (slowRatio < 0.02) {
            g_badWindows = 0;
            // Ten smooth seconds before raising the cap again.
            if (++g_goodWindows >= 10) {
                g_goodWindows = 0;
                if (g_step > 0) --g_step;
            }
        }
        else {
            g_badWindows = 0;
        }
    }

}

namespace {

    unsigned int cappedTotal(unsigned int requested) {
        auto const& cfg = minimum::config();
        unsigned int cap = minimum::effectiveParticleCap();
        if (!cfg.enabled || !cfg.capParticles || requested <= cap) {
            return requested;
        }
        ++minimum::counters().particlePoolsCapped;
        return cap;
    }

}

struct ParticleOpt : Modify<ParticleOpt, CCParticleSystemQuad> {
    bool initWithTotalParticles(unsigned int total, bool flag) {
        // Counted so the spike logger can tell "a frame was slow because it created N particle systems".
        ++minimum::counters().particleSystemsCreated;
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
