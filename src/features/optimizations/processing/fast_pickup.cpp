#include <Geode/Geode.hpp>
using namespace geode::prelude;

// Fast pickup from Algebra Dash relied on members that changed in 2.2081.
// Kept as a stub so the setting still exists and can be re-enabled later
// when bindings for the current item value path are confirmed.
// Other optimizations (empty batch skip, particle cull) provide the lag gains.

$on_mod(Loaded) {
    if (Mod::get()->getSettingValue<bool>("fast-pickup")) {
        log::debug("Minimum: Fast Pickup setting on (full hook pending binding update)");
    }
}
