#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCSpriteBatchNode.hpp>

static bool minSkipBatch() {
    return Mod::get()->getSettingValue<bool>("skip-static-batch")
        || Mod::get()->getSettingValue<bool>("performance-mode");
}

// Skip empty batch draws. Dense levels create many batch nodes with zero quads.
// Drawing them still costs setup. This removes that cost and helps under OBS/Discord load.
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
