#include <Geode/Geode.hpp>
#include <minimum.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Geode/modify/AppDelegate.hpp>

using namespace geode::prelude;

// Fast alt-tab (Windows only): skip the save that the game does when the window
// loses focus (it freezes the game for a moment on big save files). Saving when a
// level is exited or the game is closed is NOT touched.
//
// Deliberately NOT built for Android / iOS: there the OS can kill the app right
// after it goes to the background, and that save is the one that keeps your progress.

namespace {
    bool g_insideBackgroundCall = false;
}

struct AppFocusHooks : Modify<AppFocusHooks, AppDelegate> {
    void applicationDidEnterBackground() {
        g_insideBackgroundCall = true;
        AppDelegate::applicationDidEnterBackground();
        g_insideBackgroundCall = false;
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

#endif
