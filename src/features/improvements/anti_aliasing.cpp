#include <Geode/Geode.hpp>
using namespace geode::prelude;

// Anti aliasing setting is stored and logged on load.
// Full GLFW_SAMPLES hook needs platform specific setup and is Windows oriented.
// The setting is kept so users can prepare for a future restart based MSAA path.
$on_mod(Loaded) {
    auto aa = Mod::get()->getSettingValue<int64_t>("anti-aliasing");
    if (aa > 0) {
        log::info("Minimum: Anti aliasing level {} (0=off 1=x2 2=x4 3=x8 4=x16). Restart may be required.", aa);
    }
}
