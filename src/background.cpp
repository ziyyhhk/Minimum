#include <Geode/Geode.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Tab-out volume (Windows only).
//
// Driven by the same focus poll the frame gate uses (GetForegroundWindow), not by
// AppDelegate callbacks. That way the volume can never get stuck low: whenever the
// poll says the window is focused again, the saved volume is restored.

namespace minimum {

#ifdef GEODE_IS_WINDOWS

    namespace {
        bool g_dimmed = false;
        float g_savedMusic = 0.f;
        float g_savedSfx = 0.f;

        void dimAudio() {
            if (g_dimmed) return;
            auto const& cfg = config();
            if (!cfg.enabled || !cfg.backgroundVolume) return;

            auto* engine = FMODAudioEngine::sharedEngine();
            if (!engine) return;

            g_savedMusic = engine->getBackgroundMusicVolume();
            g_savedSfx = engine->getEffectsVolume();
            engine->setBackgroundMusicVolume(g_savedMusic * cfg.backgroundVolumeScale);
            engine->setEffectsVolume(g_savedSfx * cfg.backgroundVolumeScale);
            g_dimmed = true;
        }

        void restoreAudio() {
            // Restores whenever we dimmed, even if the setting was switched off in the meantime.
            if (!g_dimmed) return;

            auto* engine = FMODAudioEngine::sharedEngine();
            if (engine) {
                engine->setBackgroundMusicVolume(g_savedMusic);
                engine->setEffectsVolume(g_savedSfx);
            }
            g_dimmed = false;
        }
    }

    void onFocusChanged(bool focused) {
        if (focused) restoreAudio();
        else dimAudio();
    }

#else

    void onFocusChanged(bool) {}

#endif

}
