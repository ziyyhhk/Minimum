#include <Geode/Geode.hpp>
#include <Geode/modify/CCParticleSystem.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <algorithm>
#include <cmath>
#include <minimum.hpp>

using namespace geode::prelude;

// Particles.
//
// Two hooks, both visual only:
//
//   * CCParticleSystemQuad::draw: a particle system with zero live particles
//     still walks the GL state machine and issues a draw call every frame.
//     In levels with hundreds of placed particle objects that adds up. If
//     there is nothing to draw we simply do not draw. A system with zero
//     particles produces zero pixels either way, so this can not change what
//     you see.
//
//   * CCParticleSystem::initWithTotalParticles: every particle system
//     allocates its pool up front (GD levels love 200-500 particle pools).
//     We clamp the pool to the configured cap. The system then recycles its
//     oldest particles instead of growing, which thins extreme effects out
//     but keeps normal ones intact. The cap only applies to systems created
//     after the change, which is why the settings tell you to re-enter the
//     level.
//
// The adaptive cap: once per second the frame gate reports how many frames
// were slow. If too many were slow the cap is halved (down to 1/8 of the
// configured value); once the game is smooth again it grows back in 1.5x
// steps. New systems created after a step pick up the new cap.

namespace minimum {

    namespace {
        // Multiplier applied on top of the configured cap by the adaptive cap.
        double g_capScale = 1.0;
        // Last time the scale changed (seconds, steady clock). The scale moves
        // at most once every two seconds so it can not oscillate.
        double g_lastStep = -10.0;
    }

    unsigned int effectiveParticleCap() {
        auto const& cfg = config();
        if (!cfg.adaptiveCap) return cfg.particleCap;
        double const scaled = static_cast<double>(cfg.particleCap) * g_capScale;
        return std::max(4u, static_cast<unsigned int>(scaled + 0.5));
    }

    void noteFrameWindow(uint32_t frames, uint32_t slowFrames) {
        auto const& cfg = config();
        if (!cfg.enabled || !cfg.adaptiveCap) {
            g_capScale = 1.0;
            return;
        }
        if (frames == 0) return;

        double const now = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        if (now - g_lastStep < 2.0) return;

        double const slowRatio = static_cast<double>(slowFrames) / static_cast<double>(frames);

        if (slowRatio > 0.15 && g_capScale > 0.125) {
            // More than 15 % of frames were slow: halve the cap.
            g_capScale = std::max(0.125, g_capScale * 0.5);
            g_lastStep = now;
        }
        else if (slowRatio < 0.03 && g_capScale < 1.0) {
            // Smooth again: grow back towards the configured cap.
            g_capScale = std::min(1.0, g_capScale * 1.5);
            g_lastStep = now;
        }
    }

}

class $modify(MinimumParticleDraw, CCParticleSystemQuad) {
    void draw() {
        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.skipIdleParticles && this->getParticleCount() == 0) {
            ++minimum::counters().idleParticleDrawsSkipped;
            return;
        }
        CCParticleSystemQuad::draw();
    }
};

class $modify(MinimumParticleCap, CCParticleSystem) {
    bool initWithTotalParticles(unsigned int numberOfParticles) {
        ++minimum::counters().particleSystemsCreated;

        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.capParticles) {
            unsigned int const cap = minimum::effectiveParticleCap();
            if (numberOfParticles > cap) {
                numberOfParticles = cap;
                ++minimum::counters().particlePoolsCapped;
            }
        }
        return CCParticleSystem::initWithTotalParticles(numberOfParticles);
    }
};
