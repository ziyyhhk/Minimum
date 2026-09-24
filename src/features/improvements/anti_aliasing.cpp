#include <Geode/Geode.hpp>
using namespace geode::prelude;

$on_mod(Loaded) {
    auto aa = Mod::get()->getSettingValue<int64_t>("anti-aliasing");
    if (aa > 0) {
        // Original AlgebraDash anti-aliasing logic placeholder
        // Needs update for current Geode / GD OpenGL context
        log::info("Minimum: Anti-aliasing setting = {}", aa);
    }
}
