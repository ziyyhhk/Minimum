#include <Geode/Geode.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Fast Alt Tab (Windows).
//
// When the game window loses focus, GD writes a full save of all local data.
// That write is big enough to feel as a stutter when you alt-tab out and back
// in. The focus-loss save comes in through trySaveGame(true); the saves that
// protect your progress (level exit, closing the game) come in with false and
// are left alone.
//
// Deliberately not offered on mobile: there the background save is the only
// thing that keeps your progress when the OS kills the app.

class $modify(MinimumAppDelegate, AppDelegate) {
    void trySaveGame(bool focusLoss) {
#ifdef GEODE_IS_WINDOWS
        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.fastAltTab && focusLoss) {
            return;
        }
#endif
        AppDelegate::trySaveGame(focusLoss);
    }
};
