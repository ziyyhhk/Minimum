#include <Geode/Geode.hpp>
using namespace geode::prelude;
#include <Geode/modify/AppDelegate.hpp>

bool isUnfocusing = false;
bool isQuittingLevel = false;

struct FastAltTab : geode::Modify<FastAltTab, AppDelegate> {
    void applicationDidEnterBackground() {
        isUnfocusing = true;
        AppDelegate::applicationDidEnterBackground();
        isUnfocusing = false;
    }
    void trySaveGame(bool force) {
        bool on = Mod::get()->getSettingValue<bool>("mod-enabled");
        bool skipFocus = on && Mod::get()->getSettingValue<bool>("fast-alt-tab") && isUnfocusing;
        bool skipQuit = on && (
            Mod::get()->getSettingValue<bool>("fast-level-exit") ||
            Mod::get()->getSettingValue<bool>("performance-mode")
        ) && isQuittingLevel && !force;

        if (skipFocus || skipQuit)
            return;

        AppDelegate::trySaveGame(force);
    }

    static void onModify(auto& self) {
        (void)self.setHookPriority("AppDelegate::trySaveGame", -9999);
    }
};

// Exported for level exit path
void Minimum_setQuittingLevel(bool v) {
    isQuittingLevel = v;
}
