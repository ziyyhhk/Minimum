#include <Geode/Geode.hpp>
#include <minimum.hpp>

// Quick toggles (desktop only): keybind settings, rebindable in the mod settings.
// Mobile has no keyboard, so these settings only exist on Windows and macOS.

#ifdef GEODE_IS_DESKTOP

#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

namespace {

    void toggleSetting(char const* key, char const* label) {
        auto* mod = Mod::get();
        bool nowOn = !mod->getSettingValue<bool>(key);
        mod->setSettingValue<bool>(key, nowOn);
        minimum::refreshConfig();

        Notification::create(
            fmt::format("{}: {}", label, nowOn ? "ON" : "OFF"),
            nowOn ? NotificationIcon::Success : NotificationIcon::Info,
            1.f
        )->show();
    }

}

$on_game(Loaded) {
    listenForKeybindSettingPresses("toggle-mod-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) toggleSetting("mod-enabled", "Minimum");
    });
    listenForKeybindSettingPresses("toggle-stats-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) toggleSetting("show-stats", "Minimum stats line");
    });
}

#endif
