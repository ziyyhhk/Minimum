#include <Geode/Geode.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

$on_mod(Loaded) {
    minimum::refreshConfig();
    auto const& cfg = minimum::config();
    log::info(
        "Minimum loaded: enabled={}, particle cap={}, skip idle particle draws={}, "
        "background throttle={} ({} fps), draw divide={} ({} fps)",
        cfg.enabled,
        cfg.capParticles ? static_cast<int>(cfg.particleCap) : 0,
        cfg.skipIdleParticles,
        cfg.backgroundThrottle, cfg.backgroundFps,
        cfg.drawDivide, cfg.visualFps
    );
}
