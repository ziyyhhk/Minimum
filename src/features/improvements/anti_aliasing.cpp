#include <Geode/Geode.hpp>
using namespace geode::prelude;

// Anti aliasing is optional and costs FPS under recording / share screen.
// Default stays off. Performance Mode does not force AA on.
$on_mod(Loaded) {
    auto aa = Mod::get()->getSettingValue<int64_t>("anti-aliasing");
    if (aa > 0) {
        log::info("Minimum: Anti aliasing level {} set. Restart may be required. Turn off if recording lags.", aa);
    }
}
