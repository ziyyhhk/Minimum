#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/PlayLayer.hpp>
#include <shared_pool.hpp>

extern void Minimum_setQuittingLevel(bool v);

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minFastExit() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("fast-level-exit") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

static bool minFastEnter() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("fast-level-enter") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

static void stopParticlesRecursive(CCNode* node) {
    if (!node) return;
    if (auto* ps = typeinfo_cast<CCParticleSystem*>(node)) {
        ps->stopSystem();
        ps->setVisible(false);
    }
    auto* children = node->getChildren();
    if (!children) return;
    for (unsigned int i = 0; i < children->count(); ++i) {
        stopParticlesRecursive(static_cast<CCNode*>(children->objectAtIndex(i)));
    }
}

struct FastLevel : Modify<FastLevel, PlayLayer> {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (minFastEnter())
            Minimum::warmPool();

        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;

        if (minFastEnter())
            Minimum::warmPool();

        return true;
    }

    void onQuit() {
        if (minFastExit()) {
            Minimum_setQuittingLevel(true);
            stopParticlesRecursive(this);
        }
        PlayLayer::onQuit();
        Minimum_setQuittingLevel(false);
    }
};
