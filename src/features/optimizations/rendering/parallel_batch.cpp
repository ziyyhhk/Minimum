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

// Skip empty batch draws. Always check live setting.
struct SkipEmptyBatch : Modify<SkipEmptyBatch, CCSpriteBatchNode> {
    void draw() {
        if (minSkipBatch()) {
            auto* atlas = this->getTextureAtlas();
            if (!atlas || atlas->getTotalQuads() == 0)
                return;
        }
        CCSpriteBatchNode::draw();
    }
};
