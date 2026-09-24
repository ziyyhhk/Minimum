#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCSpriteBatchNode.hpp>

// Skip empty batch draws. On dense levels many batch nodes exist with zero quads.
// Drawing them still costs setup work. This cuts that cost.
struct SkipEmptyBatch : Modify<SkipEmptyBatch, CCSpriteBatchNode> {
    void draw() {
        if (Mod::get()->getSettingValue<bool>("skip-static-batch")) {
            auto* atlas = getTextureAtlas();
            if (!atlas || atlas->getTotalQuads() == 0)
                return;
        }
        CCSpriteBatchNode::draw();
    }
};
