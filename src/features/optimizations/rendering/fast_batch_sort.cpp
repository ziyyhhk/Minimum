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

// Faster child sort for large batches using std::stable_sort instead of insertion-style work.
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
        // For small counts, vanilla is fine
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

        // Write back order
        for (unsigned int i = 0; i < nodes.size(); ++i) {
            children->replaceObjectAtIndex(i, nodes[i], false);
        }

        // Let parent finish atlas index bookkeeping via vanilla path on a clean flag
        // We already sorted; call vanilla which should mostly update indices
        m_bReorderChildDirty = true;
        CCSpriteBatchNode::sortAllChildren();
    }
};
