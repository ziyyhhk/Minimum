#include <Geode/Geode.hpp>
#include <algorithm>
#include <string>
#include <minimum.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// Settings that only exist on some platforms (see "platforms" in mod.json) are only
// read on those platforms. Reading a setting that does not exist on the current
// platform would log an error every time the config is refreshed.

namespace minimum {

    namespace {
        Config g_config;
        Counters g_counters;
        std::chrono::steady_clock::time_point g_lastRefresh{};
        std::chrono::steady_clock::time_point g_lastFocusCheck{};
        bool g_focused = true;

        HudCorner parseCorner(std::string const& v) {
            if (v == "Top Right") return HudCorner::TopRight;
            if (v == "Bottom Left") return HudCorner::BottomLeft;
            if (v == "Bottom Right") return HudCorner::BottomRight;
            return HudCorner::TopLeft;
        }

#ifdef GEODE_IS_WINDOWS
        ProcessPriority parsePriority(std::string const& v) {
            if (v == "Above Normal") return ProcessPriority::AboveNormal;
            if (v == "High") return ProcessPriority::High;
            return ProcessPriority::Normal;
        }
#endif

        // Presets only touch the settings that trade quality for speed.
        // "Custom" leaves every individual setting alone.
        void applyPreset(std::string const& name, Config& c) {
            if (name == "Balanced") {
                c.particleCap = 128;
                c.backgroundFps = 20.0;
            }
            else if (name == "Performance") {
                c.particleCap = 64;
                c.backgroundFps = 15.0;
            }
            else if (name == "Extreme") {
                c.particleCap = 32;
                c.backgroundFps = 10.0;
            }
        }
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
        c.adaptiveCap = mod->getSettingValue<bool>("adaptive-cap");
        int64_t targetFps = mod->getSettingValue<int64_t>("adaptive-target-fps");
        c.adaptiveTargetFps = static_cast<double>(std::clamp<int64_t>(targetFps, 20, 360));

        c.spikeLogger = mod->getSettingValue<bool>("spike-logger");
        int64_t spikeMs = mod->getSettingValue<int64_t>("spike-threshold-ms");
        c.spikeThresholdMs = static_cast<double>(std::clamp<int64_t>(spikeMs, 10, 500));

        c.showStats = mod->getSettingValue<bool>("show-stats");
        c.hudDetailed = mod->getSettingValue<std::string>("hud-detail") == "Detailed";
        c.hudCorner = parseCorner(mod->getSettingValue<std::string>("hud-corner"));
        double hudScale = mod->getSettingValue<double>("hud-scale");
        c.hudScale = static_cast<float>(std::clamp(hudScale, 0.2, 2.0));
        int64_t hudOpacity = mod->getSettingValue<int64_t>("hud-opacity");
        c.hudOpacity = static_cast<uint8_t>(std::clamp<int64_t>(hudOpacity, 20, 255));

#ifdef GEODE_IS_WINDOWS
        // Windows only settings
        c.backgroundThrottle = mod->getSettingValue<bool>("background-throttle");
        int64_t bgFps = mod->getSettingValue<int64_t>("background-fps");
        c.backgroundFps = static_cast<double>(std::clamp<int64_t>(bgFps, 1, 60));
        c.backgroundVolume = mod->getSettingValue<bool>("background-volume");
        double scale = mod->getSettingValue<double>("background-volume-scale");
        c.backgroundVolumeScale = static_cast<float>(std::clamp(scale, 0.0, 1.0));

        c.drawDivide = mod->getSettingValue<bool>("draw-divide");
        double visual = mod->getSettingValue<double>("visual-fps");
        c.visualFps = std::clamp(visual, 30.0, 360.0);

        c.timerResolution = mod->getSettingValue<bool>("timer-resolution");
        c.disablePowerThrottling = mod->getSettingValue<bool>("disable-power-throttling");
        c.priority = parsePriority(mod->getSettingValue<std::string>("process-priority"));

        c.fastAltTab = mod->getSettingValue<bool>("fast-alt-tab");
#else
        c.backgroundThrottle = false;
        c.backgroundVolume = false;
        c.drawDivide = false;
        c.timerResolution = false;
        c.disablePowerThrottling = false;
        c.fastAltTab = false;
#endif

        applyPreset(mod->getSettingValue<std::string>("preset"), c);
#ifndef GEODE_IS_WINDOWS
        c.backgroundThrottle = false;
#endif

        g_config = c;
        applySystemTuning();
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
