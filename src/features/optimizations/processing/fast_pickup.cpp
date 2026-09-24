#include <Geode/Geode.hpp>
using namespace geode::prelude;

// Placeholder for future fast pickup rewrite (2.2081 bindings changed).
// Master enable and particle/batch opts are the active lag reducers.

$on_mod(Loaded) {
    if (Mod::get()->getSettingValue<bool>("mod-enabled")) {
        log::info("Minimum v1.3.0 loaded. Toggle Enable Minimum anytime with no restart.");
    }
}
