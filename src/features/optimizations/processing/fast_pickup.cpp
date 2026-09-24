#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/PlayLayer.hpp>
#include <shared_pool.hpp>

struct WarmOnPlay : Modify<WarmOnPlay, PlayLayer> {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;
        if (Mod::get()->getSettingValue<bool>("mod-enabled"))
            Minimum::warmPool();
        return true;
    }
};

$on_mod(Loaded) {
    log::info("Minimum v2.0.1 loaded.");
    if (Mod::get()->getSettingValue<bool>("mod-enabled"))
        Minimum::warmPool();
}
