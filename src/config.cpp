#include <Geode/Geode.hpp>
#include <algorithm>
#include <minimum.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace minimum {

    namespace {
        Config g_config;
        Counters g_counters;
        std::chrono::steady_clock::time_point g_lastRefresh{};
        std::chrono::steady_clock::time_point g_lastFocusCheck{};
        bool g_focused = true;
    }

    Config const& config() {
        return g_config;
    }

    Counters& counters() {
        return g_counters;
    }

    void refreshConfig() {
        auto* mod = Mod::get();

        Config c;
        c.enabled = mod->getSettingValue<bool>("mod-enabled");

        c.skipIdleParticles = mod->getSettingValue<bool>("skip-idle-particles");
        c.capParticles = mod->getSettingValue<bool>("cap-particles");
        int64_t cap = mod->getSettingValue<int64_t>("particle-cap");
        c.particleCap = static_cast<unsigned int>(std::clamp<int64_t>(cap, 4, 1000));

        c.backgroundThrottle = mod->getSettingValue<bool>("background-throttle");
        int64_t bgFps = mod->getSettingValue<int64_t>("background-fps");
        c.backgroundFps = static_cast<double>(std::clamp<int64_t>(bgFps, 1, 60));
        c.backgroundVolume = mod->getSettingValue<bool>("background-volume");
        double scale = mod->getSettingValue<double>("background-volume-scale");
        c.backgroundVolumeScale = static_cast<float>(std::clamp(scale, 0.0, 1.0));

        c.drawDivide = mod->getSettingValue<bool>("draw-divide");
        double visual = mod->getSettingValue<double>("visual-fps");
        c.visualFps = std::clamp(visual, 30.0, 360.0);

        c.fastAltTab = mod->getSettingValue<bool>("fast-alt-tab");
        c.showStats = mod->getSettingValue<bool>("show-stats");

        g_config = c;
    }

    void refreshConfigIfDue(std::chrono::steady_clock::time_point now) {
        using namespace std::chrono_literals;
        if (now - g_lastRefresh >= 500ms) {
            g_lastRefresh = now;
            refreshConfig();
        }
    }

    bool processHasForeground(std::chrono::steady_clock::time_point now) {
#ifdef GEODE_IS_WINDOWS
        using namespace std::chrono_literals;
        if (now - g_lastFocusCheck >= 250ms) {
            g_lastFocusCheck = now;
            HWND foreground = GetForegroundWindow();
            DWORD ownerPid = 0;
            if (foreground) {
                GetWindowThreadProcessId(foreground, &ownerPid);
            }
            g_focused = foreground != nullptr && ownerPid == GetCurrentProcessId();
        }
        return g_focused;
#else
        (void)now;
        return true;
#endif
    }

}
