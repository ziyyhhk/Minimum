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
        FrameStats g_frameStats;
        std::chrono::steady_clock::time_point g_lastRefresh{};
        std::chrono::steady_clock::time_point g_lastFocusCheck{};
        bool g_focused = true;
        int g_unfocusedPolls = 0;

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

        // A preset only touches the settings that trade quality for speed. "Custom" leaves
        // every individual setting alone. The numbers live in minimum_logic.hpp.
        void applyPreset(PresetValues const& preset, Config& c) {
            if (!preset.known) return;
            c.skipIdleParticles = true;
            c.capParticles = true;
            c.particleCap = preset.particleCap;
            if (preset.lowDetail) c.lowDetail = true;
#ifdef GEODE_IS_WINDOWS
            c.backgroundThrottle = true;
            c.backgroundFps = preset.backgroundFps;
#endif
        }
    }

    Config const& config() {
        return g_config;
    }

    Counters& counters() {
        return g_counters;
    }

    FrameStats& frameStats() {
        return g_frameStats;
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
        c.targetFps = static_cast<double>(std::clamp<int64_t>(targetFps, 20, 360));

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

        c.lowLatency = mod->getSettingValue<bool>("low-latency");
        c.lowDetail = mod->getSettingValue<bool>("low-detail");

#ifdef GEODE_IS_ANDROID
        c.androidCpuHint = mod->getSettingValue<bool>("android-cpu-hint");
#else
        c.androidCpuHint = false;
#endif

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

        applyPreset(presetValues(mod->getSettingValue<std::string>("preset")), c);

        g_config = c;
        applySystemTuning();
    }

    void leavePreset() {
        auto* mod = Mod::get();
        auto const preset = presetValues(mod->getSettingValue<std::string>("preset"));
        if (!preset.known) return;

        // Copy what the preset was doing into the real settings first, so nothing changes
        // for the player at the moment the preset is dropped.
        mod->setSettingValue<bool>("skip-idle-particles", true);
        mod->setSettingValue<bool>("cap-particles", true);
        mod->setSettingValue<int64_t>("particle-cap", static_cast<int64_t>(preset.particleCap));
        if (preset.lowDetail) mod->setSettingValue<bool>("low-detail", true);
#ifdef GEODE_IS_WINDOWS
        mod->setSettingValue<bool>("background-throttle", true);
        mod->setSettingValue<int64_t>("background-fps", static_cast<int64_t>(preset.backgroundFps));
#endif
        mod->setSettingValue<std::string>("preset", "Custom");
        refreshConfig();
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

            // Two independent checks. The window counts as focused if EITHER says so:
            //  * GetFocus() is per thread: it is non-null while a window of the game's
            //    own (main) thread owns the keyboard focus. Works the same under Wine.
            //  * the foreground window belongs to this process.
            // Unfocused is only reported after two unfocused polls in a row (about 0.5 s),
            // so a one-off wrong answer can never throttle the game or dim the audio.
            bool ownsKeyboardFocus = GetFocus() != nullptr;

            bool foregroundIsOurs = false;
            if (HWND foreground = GetForegroundWindow()) {
                DWORD ownerPid = 0;
                GetWindowThreadProcessId(foreground, &ownerPid);
                foregroundIsOurs = ownerPid == GetCurrentProcessId();
            }

            if (ownsKeyboardFocus || foregroundIsOurs) {
                g_unfocusedPolls = 0;
                g_focused = true;
            }
            else if (++g_unfocusedPolls >= 2) {
                g_focused = false;
            }
        }
        return g_focused;
#else
        (void)now;
        return true;
#endif
    }

}
