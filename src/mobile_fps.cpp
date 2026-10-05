#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <algorithm>
#include <minimum.hpp>

using namespace geode::prelude;

// FPS unlock (Android / iOS).
//
// Mobile GD asks the OS for a new frame every 1/60 s through
// CCDirector::setAnimationInterval, no matter what your screen can do. We let
// that request go through unchanged unless it is SLOWER than the configured
// limit, in which case we ask for a faster frame instead. The compositor and
// vsync still decide the real maximum: on a 60 Hz phone nothing changes, on a
// 90/120 Hz screen the game can finally use it.

#ifdef GEODE_IS_MOBILE

class $modify(MinimumFpsUnlock, CCDirector) {
    void setAnimationInterval(double interval) {
        auto const& cfg = minimum::config();
        if (cfg.enabled && cfg.fpsUnlock) {
            double const wanted = 1.0 / std::max(30.0, cfg.fpsLimit);
            if (interval > wanted) interval = wanted;
        }
        CCDirector::setAnimationInterval(interval);
    }
};

#endif
