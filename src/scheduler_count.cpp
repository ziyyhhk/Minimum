#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Counts game logic steps (scheduler updates). Frames drawn and logic steps are not always the
// same number: the game can update 240 times per second and draw 60 frames. Other FPS counters
// often show one of the two, so Minimum measures both and shows the logic rate next to the FPS
// when they are clearly different. The hook does nothing except add one to a counter.

struct SchedulerCount : Modify<SchedulerCount, CCScheduler> {
    void update(float dt) {
        minimum::counters().schedulerUpdates.fetch_add(1, std::memory_order_relaxed);
        CCScheduler::update(dt);
    }
};
