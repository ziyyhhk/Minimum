#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/ui/Notification.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Hotkeys (Windows / macOS). The keys themselves are configured in the mod
// settings ("type": "keybind"); here we only listen for the press.

#ifdef GEODE_IS_DESKTOP

namespace {
    void flipBoolSetting(char const* key, char const* name) {
        auto* mod = Mod::get();
        bool const next = !mod->getSettingValue<bool>(key);
        mod->setSettingValue<bool>(key, next);
        minimum::refreshConfig();

        if (auto* note = Notification::create(
            fmt::format("{}: {}", name, next ? "ON" : "OFF"),
            NotificationIcon::Info
        )) {
            note->show();
        }
    }
}

$on_game(Loaded) {
    listenForKeybindSettingPresses(
        "toggle-mod-key",
        [](Keybind const&, bool down, bool repeat, double) {
            if (down && !repeat) flipBoolSetting("mod-enabled", "Minimum");
        }
    );
    listenForKeybindSettingPresses(
        "toggle-stats-key",
        [](Keybind const&, bool down, bool repeat, double) {
            if (down && !repeat) flipBoolSetting("show-stats", "FPS counter");
        }
    );
}

#endif
