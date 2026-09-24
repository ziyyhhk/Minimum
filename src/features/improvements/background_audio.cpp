#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/AppDelegate.hpp>

// On Windows, alt-tab fires applicationWillResignActive / applicationWillBecomeActive,
// NOT enter background. That is why volume never dropped before.

static float g_savedMusic = -1.f;
static float g_savedSfx = -1.f;
static bool g_dimmed = false;

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minBgAudio() {
    return minOn() && Mod::get()->getSettingValue<bool>("background-volume");
}

static float minBgScale() {
    float s = static_cast<float>(Mod::get()->getSettingValue<double>("background-volume-scale"));
    if (s < 0.f) s = 0.f;
    if (s > 1.f) s = 1.f;
    return s;
}

static void dimAudio() {
    if (g_dimmed) return;
    if (!minBgAudio()) return;

    auto* eng = FMODAudioEngine::sharedEngine();
    if (!eng) return;

    g_savedMusic = eng->getBackgroundMusicVolume();
    g_savedSfx = eng->getEffectsVolume();
    float scale = minBgScale();

    // Write both the engine members and channel volumes
    eng->setBackgroundMusicVolume(g_savedMusic * scale);
    eng->setEffectsVolume(g_savedSfx * scale);
    g_dimmed = true;

    log::debug("Minimum: volume dimmed to {}%", static_cast<int>(scale * 100));
}

static void restoreAudio() {
    if (!g_dimmed) return;

    auto* eng = FMODAudioEngine::sharedEngine();
    if (!eng) return;

    if (g_savedMusic >= 0.f)
        eng->setBackgroundMusicVolume(g_savedMusic);
    if (g_savedSfx >= 0.f)
        eng->setEffectsVolume(g_savedSfx);

    g_dimmed = false;
    g_savedMusic = -1.f;
    g_savedSfx = -1.f;

    log::debug("Minimum: volume restored");
}

struct BackgroundAudio : Modify<BackgroundAudio, AppDelegate> {
    // Windows alt-tab
    void applicationWillResignActive() {
        dimAudio();
        AppDelegate::applicationWillResignActive();
    }

    void applicationWillBecomeActive() {
        restoreAudio();
        AppDelegate::applicationWillBecomeActive();
    }

    // Mobile / true background
    void applicationDidEnterBackground() {
        dimAudio();
        AppDelegate::applicationDidEnterBackground();
    }

    void applicationWillEnterForeground() {
        restoreAudio();
        AppDelegate::applicationWillEnterForeground();
    }
};
