#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <shared_pool.hpp>

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minSkipEmpty() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("skip-empty-batch") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

static bool minParallelBatch() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("parallel-batch") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

static int minParallelThreshold() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("parallel-threshold"));
}

// Empty batch skip + optional parallel child transform update before draw.
struct OptSpriteBatch : Modify<OptSpriteBatch, CCSpriteBatchNode> {
    void draw() {
        auto* atlas = this->getTextureAtlas();
        if (minSkipEmpty() && (!atlas || atlas->getTotalQuads() == 0))
            return;

        if (minParallelBatch()) {
            auto* children = this->getChildren();
            if (children) {
                unsigned int n = children->count();
                int threshold = minParallelThreshold();
                if (threshold < 32) threshold = 32;
                if (n >= static_cast<unsigned int>(threshold)) {
                    CCObject** arr = children->data->arr;
                    Minimum::pool().parallelFor(0, n, 64, [arr](size_t a, size_t b) {
                        for (size_t i = a; i < b; ++i) {
                            auto* spr = static_cast<CCSprite*>(arr[i]);
                            if (spr) spr->updateTransform();
                        }
                    });
                }
            }
        }

        CCSpriteBatchNode::draw();
    }
};
