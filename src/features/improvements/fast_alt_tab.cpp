#include <Geode/Geode.hpp>
using namespace geode::prelude;
#include <Geode/modify/AppDelegate.hpp>

bool isUnfocusing = false;
struct FastAltTab : geode::Modify<FastAltTab, AppDelegate> {
    void applicationDidEnterBackground() {
        isUnfocusing = true;
        AppDelegate::applicationDidEnterBackground();
        isUnfocusing = false;
    }
    void trySaveGame(bool force) {
        bool on = Mod::get()->getSettingValue<bool>("mod-enabled")
            && Mod::get()->getSettingValue<bool>("fast-alt-tab");
        if (on && isUnfocusing)
            return;
        AppDelegate::trySaveGame(force);
    }

    static void onModify(auto& self) {
        self.setHookPriority("AppDelegate::trySaveGame", -9999);
    }
};
