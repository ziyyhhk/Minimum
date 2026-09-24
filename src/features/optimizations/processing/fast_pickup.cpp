#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <shared_pool.hpp>

$on_mod(Loaded) {
    log::info("Minimum v2.0.2 loaded.");
    if (Mod::get()->getSettingValue<bool>("mod-enabled"))
        Minimum::warmPool();
}
