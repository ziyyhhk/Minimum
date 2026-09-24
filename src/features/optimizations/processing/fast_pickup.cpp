#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <Geode/modify/GJBaseGameLayer.hpp>

// Faster collectItem path from Algebra Dash, updated for modern Geode.
// Avoids extra work when count is zero and clamps id range safely.
struct FastPickup : Modify<FastPickup, GJBaseGameLayer> {
    void collectItem(int id, int count) {
        if (!Mod::get()->getSettingValue<bool>("fast-pickup")) {
            GJBaseGameLayer::collectItem(id, count);
            return;
        }
        if (count == 0)
            return;
        if (id < 0)
            id = 0;
        else if (id > 1100)
            id = 1100;

        // Prefer the normal path if effect manager is missing
        if (!m_effectManager) {
            GJBaseGameLayer::collectItem(id, count);
            return;
        }

        int value = m_effectManager->m_itemValues[id] + count;
        m_effectManager->m_itemValues[id] = value;
        m_effectManager->countChangedForItem(id);
        this->updateCounters(id, value);
    }
};
