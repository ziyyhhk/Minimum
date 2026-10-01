#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCSpriteBatchNode.hpp>

// Parallel transforms were removed: Cocos2d nodes are not thread-safe.
// Calling updateTransform from worker threads caused access violations
// and mutex crashes (Mtx_lock) under load. Empty-batch skip stays.

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minSkipEmpty() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("skip-empty-batch") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

struct OptSpriteBatch : Modify<OptSpriteBatch, CCSpriteBatchNode> {
    void draw() {
        auto* atlas = this->getTextureAtlas();
        if (minSkipEmpty() && (!atlas || atlas->getTotalQuads() == 0))
            return;

        CCSpriteBatchNode::draw();
    }
};
