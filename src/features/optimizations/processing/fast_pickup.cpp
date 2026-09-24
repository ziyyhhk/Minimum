#include <Geode/Geode.hpp>
using namespace geode::prelude;

// Fast pickup from Algebra Dash used members that changed in 2.2081.
// Setting remains for future reimplementation when bindings are confirmed.
// Performance Mode and particle/batch opts provide the main lag reduction for now.

$on_mod(Loaded) {
    if (Mod::get()->getSettingValue<bool>("performance-mode")) {
        log::info("Minimum: Performance Mode ON (strong particle cull, batch skip, particle cap)");
    }
}
