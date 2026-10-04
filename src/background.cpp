#include <Geode/Geode.hpp>
#include <cmath>
#include <minimum.hpp>

using namespace geode::prelude;

// Tab-out volume (Windows only).
//
// Why the old versions could leave the volume stuck low:
//   * They read the game's music / SFX volume, lowered it, and wrote the saved values
//     back on return. If the game changed those values in between (or the getter
//     returned something else than the slider value), the "restored" volume was wrong.
//   * They only restored on a focus *change event*. One missed event and the volume
//     stayed low for the rest of the session.
//
// This version never touches the game's own volume values. It scales FMOD's master
// channel group instead (a separate multiplier on top of everything the game plays),
// and it is stateless: audioPoll() is called every frame with the current focus state
// and simply makes the master volume match what it should be right now. If a call
// fails (engine not ready), the next frame tries again.

namespace minimum {

#ifdef GEODE_IS_WINDOWS

    namespace {
        bool g_dimmed = false;
        float g_beforeDim = 1.f;
        float g_appliedScale = 1.f;

        FMOD::ChannelGroup* masterGroup() {
            auto* engine = FMODAudioEngine::sharedEngine();
            if (!engine || !engine->m_system) return nullptr;

            FMOD::ChannelGroup* group = nullptr;
            if (engine->m_system->getMasterChannelGroup(&group) != FMOD_OK) return nullptr;
            return group;
        }
    }

    void audioPoll(bool focused) {
        auto const& cfg = config();
        bool const wantDim = cfg.enabled && cfg.backgroundVolume && !focused;

        if (wantDim) {
            auto* group = masterGroup();
            if (!group) return;

            if (!g_dimmed) {
                float current = 1.f;
                if (group->getVolume(&current) != FMOD_OK) return;
                // Never remember a (nearly) silent volume as the one to go back to.
                g_beforeDim = current > 0.05f ? current : 1.f;
                if (group->setVolume(g_beforeDim * cfg.backgroundVolumeScale) != FMOD_OK) return;
                g_appliedScale = cfg.backgroundVolumeScale;
                g_dimmed = true;
            }
            else if (std::fabs(g_appliedScale - cfg.backgroundVolumeScale) > 0.001f) {
                // The scale setting was changed while tabbed out.
                if (group->setVolume(g_beforeDim * cfg.backgroundVolumeScale) == FMOD_OK) {
                    g_appliedScale = cfg.backgroundVolumeScale;
                }
            }
        }
        else if (g_dimmed) {
            // Focused again, or the feature was switched off while dimmed: always go back.
            auto* group = masterGroup();
            if (!group) return;
            if (group->setVolume(g_beforeDim) == FMOD_OK) {
                g_dimmed = false;
            }
        }
    }

#else

    void audioPoll(bool) {}

#endif

}
