#include <Geode/Geode.hpp>
#include <minimum.hpp>

#ifdef GEODE_IS_MOBILE
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#endif

using namespace geode::prelude;

// Frame rate unlock (Android and iOS, opt-in: "fps-unlock" / "fps-limit").
//
// The game asks the system for a frame interval through setAnimationInterval, normally 1/60 s.
// While the unlock is on, that request is replaced by 1 / "FPS Limit" when that is shorter
// (minimum::unlockedInterval). A request that is already faster is left alone. When the
// unlock is off the game's own request goes through unchanged.
//
// Why CCDisplayLinkDirector and not CCDirector: in cocos2d-x 2.2.x CCDirector::setAnimationInterval is
// pure virtual and CCDisplayLinkDirector (the director GD runs on) holds the real one, so that is
// the function that can be hooked and called through.
//
// This only changes how often a frame is requested. The scheduler delta, game timing and physics are
// not touched. Whether the game really runs at the same speed at a higher frame rate has to be
// confirmed in game (see the report / README); the system may also still cap the rate (screen
// refresh rate, battery saver, and on iOS the display link frame interval is a whole number of
// 60 Hz steps).
//
// Reads the Config snapshot, never Mod::getSettingValue.

namespace minimum {

#ifdef GEODE_IS_MOBILE

    namespace {
        // The last interval the game itself asked for. 1/60 until we see a request, which is what
        // the game asks for by default.
        double g_requested = 1.0 / 60.0;
        bool g_appliedWant = false;
        int g_appliedLimit = 0;
    }

    void applyFrameUnlock() {
        auto const& cfg = config();
        bool const want = cfg.enabled && cfg.fpsUnlock;
        int const limit = want ? cfg.fpsLimit : 0;
        if (want == g_appliedWant && limit == g_appliedLimit) return;

        g_appliedWant = want;
        g_appliedLimit = limit;

        // Ask again with the game's own value: the hook below turns it into the right interval
        // for the new setting (or puts the original one back when the unlock was switched off).
        if (auto* director = CCDirector::get()) {
            director->setAnimationInterval(g_requested);
        }
    }

#else

    void applyFrameUnlock() {}

#endif

}

#ifdef GEODE_IS_MOBILE

struct FpsUnlock : Modify<FpsUnlock, CCDisplayLinkDirector> {
    void setAnimationInterval(double interval) {
        if (interval > 0.0) minimum::g_requested = interval;

        auto const& cfg = minimum::config();
        CCDisplayLinkDirector::setAnimationInterval(
            minimum::unlockedInterval(interval, cfg.enabled && cfg.fpsUnlock, cfg.fpsLimit)
        );
    }
};

#endif
