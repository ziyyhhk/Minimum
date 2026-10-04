#include <Geode/Geode.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Window focus hooks, kept in ONE modify class on purpose.
//
//   * Tab-out volume: on Windows alt-tab fires applicationWillResignActive /
//     applicationWillBecomeActive, on mobile and when minimised the Background /
//     Foreground pair. Both pairs dim and restore the volume.
//   * Fast alt-tab: skip the save that the game does when the window loses
//     focus (it freezes the game for a moment on big save files). Saving when
//     a level is exited or the game is closed is NOT touched.

namespace {

    bool g_insideBackgroundCall = false;
    bool g_dimmed = false;
    float g_savedMusic = 0.f;
    float g_savedSfx = 0.f;

    void dimAudio() {
        if (g_dimmed) return;
        auto const& cfg = minimum::config();
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

struct AppFocusHooks : Modify<AppFocusHooks, AppDelegate> {
    void applicationWillResignActive() {
        dimAudio();
        AppDelegate::applicationWillResignActive();
    }

    void applicationWillBecomeActive() {
        restoreAudio();
        AppDelegate::applicationWillBecomeActive();
    }

    void applicationDidEnterBackground() {
        dimAudio();
        g_insideBackgroundCall = true;
        AppDelegate::applicationDidEnterBackground();
        g_insideBackgroundCall = false;
    }

    void applicationWillEnterForeground() {
        restoreAudio();
        AppDelegate::applicationWillEnterForeground();
    }

    void trySaveGame(bool force) {
        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.fastAltTab && g_insideBackgroundCall) {
            return;
        }
        AppDelegate::trySaveGame(force);
    }

    static void onModify(auto& self) {
        // Run before other mods' trySaveGame hooks so the skip really skips.
        (void)self.setHookPriority("AppDelegate::trySaveGame", -9999);
    }
};
