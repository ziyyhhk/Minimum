#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/PlayLayer.hpp>
#include <shared_pool.hpp>

// On level start, warm the thread pool so first parallel batch has workers ready.
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
    log::info("Minimum v2.0.0 loaded. Parallel batch + particle + streak opts ready.");
}
