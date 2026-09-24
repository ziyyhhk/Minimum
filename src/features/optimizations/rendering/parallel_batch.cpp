#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCSpriteBatchNode.hpp>

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minSkipBatch() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("skip-static-batch") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

// Skip empty batch draws. Live toggle via Enable Minimum / Skip Empty Batches.
struct SkipEmptyBatch : Modify<SkipEmptyBatch, CCSpriteBatchNode> {
    void draw() {
        if (minSkipBatch()) {
            auto* atlas = getTextureAtlas();
            if (!atlas || atlas->getTotalQuads() == 0)
                return;
        }
        CCSpriteBatchNode::draw();
    }
};
