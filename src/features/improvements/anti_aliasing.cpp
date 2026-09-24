#include <Geode/Geode.hpp>
using namespace geode::prelude;

$on_mod(Loaded) {
    auto aa = Mod::get()->getSettingValue<int64_t>("anti-aliasing");
    if (aa > 0)
        log::info("Minimum: AA level {}. Set to 0 for max FPS.", aa);
}
