#include <Geode/Geode.hpp>
using namespace geode::prelude;

$on_mod(Loaded) {
    auto aa = Mod::get()->getSettingValue<int64_t>("anti-aliasing");
    if (aa > 0) {
        log::info("Minimum: Anti aliasing level {}. Keep at 0 for best FPS while recording.", aa);
    }
}
