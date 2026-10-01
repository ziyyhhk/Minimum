#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/CCSpriteBatchNode.hpp>
#include <algorithm>
#include <vector>

static bool minOn() {
    return Mod::get()->getSettingValue<bool>("mod-enabled");
}

static bool minFastSort() {
    return minOn() && (
        Mod::get()->getSettingValue<bool>("fast-batch-sort") ||
        Mod::get()->getSettingValue<bool>("performance-mode")
    );
}

// Faster child sort for large batches using std::stable_sort.
// Avoid calling vanilla sort again after we already sorted (was wasteful and could mess indices).
struct FastBatchSort : Modify<FastBatchSort, CCSpriteBatchNode> {
    void sortAllChildren() {
        if (!minFastSort()) {
            CCSpriteBatchNode::sortAllChildren();
            return;
        }

        if (!m_bReorderChildDirty)
            return;

        auto* children = this->getChildren();
        if (!children || children->count() < 2) {
            m_bReorderChildDirty = false;
            return;
        }

        unsigned int n = children->count();
        // Small counts: vanilla is fine and keeps atlas bookkeeping simple
        if (n < 64) {
            CCSpriteBatchNode::sortAllChildren();
            return;
        }

        std::vector<CCNode*> nodes;
        nodes.reserve(n);
        for (unsigned int i = 0; i < n; ++i) {
            if (auto* node = static_cast<CCNode*>(children->objectAtIndex(i)))
                nodes.push_back(node);
        }

        std::stable_sort(nodes.begin(), nodes.end(), [](CCNode* a, CCNode* b) {
            if (a->getZOrder() != b->getZOrder())
                return a->getZOrder() < b->getZOrder();
            return a->getOrderOfArrival() < b->getOrderOfArrival();
        });

        for (unsigned int i = 0; i < nodes.size(); ++i) {
            children->replaceObjectAtIndex(i, nodes[i], false);
        }

        // Let vanilla finish atlas index bookkeeping with a clean dirty flag
        m_bReorderChildDirty = true;
        CCSpriteBatchNode::sortAllChildren();
    }
};
