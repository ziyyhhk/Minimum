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
//
// 3. Adaptive cap (opt-in). If the game keeps missing the target FPS, the cap is
//    lowered in steps (cap, cap/2, cap/4, cap/8, never below 16) and raised again
//    after the game has been smooth for a while. Only affects systems created
//    afterwards, which in a level is constantly (orbs, pads, death effects, trails).
//    The decision itself lives in minimum_logic.hpp (AdaptiveCap) and is unit tested.
//
// 4. Thermal response (Android). While the device reports a severe thermal status the cap
//    is lowered by one more step, and given back after 30 s of nominal temperature
//    (ThermalGuard in minimum_logic.hpp).
//
// 5. Off-screen culling. The draw of a system whose particles are all clearly outside the
//    screen is skipped. update() is never skipped, so emission and particle lifetimes run
//    exactly as before and nothing "restarts" when the system comes back into view.
//    The bounds come from the quads the system already built for drawing, so they are what
//    would have been drawn, whatever the position type (free / relative / grouped).
//
// 6. Spawn budget (opt-in). Caps how many systems are created per frame during a level.
//    Systems over the budget are still created (the game does not expect a failed create),
//    just with a tiny pool.
//
// All of this is visual only: nothing here touches gameplay, timing or physics.

namespace minimum {

    namespace {
        AdaptiveCap g_adaptive;
        ThermalGuard g_thermal;
    }

    int currentCapStep() {
        return combinedCapStep(g_adaptive.step(), g_thermal.steps());
    }

    unsigned int effectiveParticleCap() {
        return capForStep(config().particleCap, currentCapStep());
    }

    void noteFrameWindow(uint32_t drawnFrames, uint32_t slowFrames) {
        auto const& cfg = config();
        if (!cfg.enabled || !cfg.capParticles || !cfg.adaptiveCap) {
            g_adaptive.reset();
            return;
        }
        g_adaptive.update(drawnFrames, slowFrames);
    }

    void noteThermalStatus(int status, double nowSeconds) {
        auto const& cfg = config();
        if (!cfg.enabled || !cfg.capParticles) {
            g_thermal.reset();
        }
        else {
            g_thermal.update(status, nowSeconds);
        }
        counters().thermal.store(g_thermal.throttling(), std::memory_order_relaxed);
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

    // Per frame accounting for the spawn budget. "Frame" = one call of the frame hook.
    uint64_t g_budgetFrame = ~0ull;
    uint64_t g_budgetBase = 0;

    // `createdBefore` is how many systems existed (were created) before the one being created.
    unsigned int budgetedTotal(unsigned int requested, uint64_t createdBefore) {
        auto const& cfg = minimum::config();
        if (!cfg.enabled || cfg.spawnBudget <= 0) return requested;

        auto& counters = minimum::counters();
        // Only during a level, and not while it loads (see kSpawnBudgetGraceFrames).
        if (!minimum::spawnBudgetActive(counters.framesInLevel.load(std::memory_order_relaxed))) return requested;

        uint64_t const frame = counters.frameIndex.load(std::memory_order_relaxed);
        if (frame != g_budgetFrame) {
            g_budgetFrame = frame;
            g_budgetBase = createdBefore;
        }
        unsigned int const total = minimum::spawnBudgetTotal(requested, createdBefore - g_budgetBase, cfg.spawnBudget);
        if (total < requested) ++counters.spawnsBudgeted;
        return total;
    }

}

struct ParticleOpt : Modify<ParticleOpt, CCParticleSystemQuad> {
    bool initWithTotalParticles(unsigned int total, bool flag) {
        // Counted so the spike logger can tell "a frame was slow because it created N particle systems",
        // and so the spawn budget knows how many were created earlier in this frame.
        uint64_t const createdBefore = minimum::counters().particleSystemsCreated.fetch_add(1, std::memory_order_relaxed);
        return CCParticleSystemQuad::initWithTotalParticles(budgetedTotal(cappedTotal(total), createdBefore), flag);
    }

    void setTotalParticles(unsigned int total) {
        CCParticleSystemQuad::setTotalParticles(cappedTotal(total));
    }

    // True when every live particle of this system is clearly outside the screen.
    //
    // Reads the quads the system built in its last update(): those are exactly what draw()
    // would send to the GPU, in the node's local space. Their bounding box is moved to world
    // space with the node's local-to-world transform (read with three point conversions, which
    // is exact for the 2D transforms nodes have) and compared with the screen plus a margin.
    // When anything is not as expected (no quads, nothing alive, non finite numbers) it says
    // "not off screen" and the system is drawn as usual.
    bool particlesOffscreen() {
        unsigned int const count = std::min<unsigned int>(m_uParticleCount, m_uAllocatedParticles);
        if (m_pQuads == nullptr || count == 0) return false;

        minimum::BoundsAccumulator bounds;
        for (unsigned int i = 0; i < count; ++i) {
            auto const& quad = m_pQuads[i];
            bounds.add(quad.bl.vertices.x, quad.bl.vertices.y);
            bounds.add(quad.br.vertices.x, quad.br.vertices.y);
            bounds.add(quad.tl.vertices.x, quad.tl.vertices.y);
            bounds.add(quad.tr.vertices.x, quad.tr.vertices.y);
        }

        CCPoint const origin = this->convertToWorldSpace({0.f, 0.f});
        CCPoint const unitX = this->convertToWorldSpace({1.f, 0.f});
        CCPoint const unitY = this->convertToWorldSpace({0.f, 1.f});
        minimum::Affine const toWorld = minimum::affineFromUnitPoints(
            {origin.x, origin.y}, {unitX.x, unitX.y}, {unitY.x, unitY.y}
        );

        CCSize const win = CCDirector::get()->getWinSize();
        minimum::Box const visible{0.f, 0.f, win.width, win.height};
        float const margin = minimum::particleCullMargin(m_fStartSize, m_fStartSizeVar, m_fEndSize, m_fEndSizeVar);
        return minimum::shouldCullParticles(true, bounds, toWorld, visible, margin);
    }

    void draw() {
        auto const& cfg = minimum::config();
        if (cfg.enabled) {
            if (cfg.skipIdleParticles && m_uParticleCount == 0) {
                ++minimum::counters().idleParticleDrawsSkipped;
                return;
            }
            // Draw only. update() still runs for this system, so nothing about it changes.
            if (cfg.cullOffscreenParticles && m_uParticleCount > 0 && this->particlesOffscreen()) {
                ++minimum::counters().particleDrawsCulled;
                return;
            }
        }
        CCParticleSystemQuad::draw();
    }
};
