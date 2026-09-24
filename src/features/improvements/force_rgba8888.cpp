#include <Geode/Geode.hpp>
using namespace geode::prelude;

// Force RGBA8888 - original logic from AlgebraDash
// Requires update for current texture loading paths in GD 2.208+
$on_mod(Loaded) {
    if (Mod::get()->getSettingValue<bool>("force-rgba8888")) {
        log::info("Minimum: Force RGBA8888 enabled (needs texture reload)");
    }
}
