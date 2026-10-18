// Tests for include/minimum_logic.hpp. No Geode SDK needed, any C++20 compiler works:
//
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude tests/logic_test.cpp -o logic_test && ./logic_test

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <minimum_logic.hpp>

namespace {

    int g_checks = 0;
    int g_failed = 0;

    void check(bool ok, char const* what, int line) {
        ++g_checks;
        if (!ok) {
            ++g_failed;
            std::printf("FAIL line %d: %s\n", line, what);
        }
    }

#define CHECK(cond) check((cond), #cond, __LINE__)

    bool near(double a, double b, double eps = 1e-6) {
        return std::fabs(a - b) <= eps;
    }

    bool inside(minimum::Box const& b, float w, float h) {
        return b.x >= 0.f && b.y >= 0.f && b.right() <= w + 0.001f && b.top() <= h + 0.001f;
    }

    bool overlap(minimum::Box const& a, minimum::Box const& b) {
        return minimum::boxesCollide(a, b, 0.f);
    }

    void testFrameStats() {
        minimum::FrameStats s;
        CHECK(s.size() == 0);
        CHECK(near(s.averageFrameMs(10), 0.0));
        CHECK(near(s.lowFps(10), 0.0));

        // 100 steady 10 ms frames.
        for (int i = 0; i < 100; ++i) s.push(10.0, 4.0);
        CHECK(s.size() == 100);
        CHECK(near(s.averageFrameMs(100), 10.0));
        CHECK(near(s.averageCpuMs(100), 4.0));
        CHECK(near(s.lowFps(100), 100.0));

        // One 50 ms hitch among 100 frames: the 1% low (the single worst frame here) must see
        // it, the average barely does.
        minimum::FrameStats h;
        for (int i = 0; i < 99; ++i) h.push(10.0, 4.0);
        h.push(50.0, 4.0);
        CHECK(h.size() == 100);
        CHECK(near(h.lowFps(100), 20.0));
        CHECK(near(h.averageFrameMs(100), 10.4));
        CHECK(h.averageFrameMs(100) < 11.0);

        // 101 frames: 1% rounds up to the two worst frames (50 ms and 10 ms).
        s.push(50.0, 4.0);
        CHECK(s.size() == 101);
        CHECK(near(s.lowFps(101), 1000.0 / 30.0, 1e-3));

        // Newest sample is last.
        CHECK(near(s.frameMsAt(s.size() - 1), 50.0));
        CHECK(near(s.frameMsAt(0), 10.0));

        // Ring wraps: after 3 * capacity pushes only the newest capacity frames remain.
        minimum::FrameStats r;
        for (size_t i = 0; i < minimum::FrameStats::kCapacity * 3; ++i) {
            r.push(static_cast<double>(i), 1.0);
        }
        CHECK(r.size() == minimum::FrameStats::kCapacity);
        CHECK(near(r.frameMsAt(r.size() - 1), static_cast<double>(minimum::FrameStats::kCapacity * 3 - 1)));
        CHECK(near(r.frameMsAt(0), static_cast<double>(minimum::FrameStats::kCapacity * 2)));

        // Asking for more than there is must not read garbage.
        minimum::FrameStats few;
        few.push(20.0, 1.0);
        few.push(40.0, 3.0);
        CHECK(near(few.averageFrameMs(1000), 30.0));
        CHECK(near(few.averageFrameMs(1), 40.0));
        CHECK(near(few.averageCpuMs(2), 2.0));

        few.clear();
        CHECK(few.size() == 0);
    }

    void testBands() {
        using minimum::FpsBand;
        CHECK(minimum::classifyFps(144.0, 60.0) == FpsBand::Good);
        CHECK(minimum::classifyFps(60.0, 60.0) == FpsBand::Good);
        CHECK(minimum::classifyFps(56.0, 60.0) == FpsBand::Good);
        CHECK(minimum::classifyFps(45.0, 60.0) == FpsBand::Okay);
        CHECK(minimum::classifyFps(20.0, 60.0) == FpsBand::Bad);
        CHECK(minimum::classifyFps(120.0, 144.0) == FpsBand::Okay);
        CHECK(minimum::classifyFps(30.0, 30.0) == FpsBand::Good);
        // A broken target falls back to 60.
        CHECK(minimum::classifyFps(60.0, 0.0) == FpsBand::Good);
    }

    void testLogicRate() {
        CHECK(!minimum::logicRateDiffers(60.0, 60.0));
        CHECK(!minimum::logicRateDiffers(60.0, 62.0));
        CHECK(!minimum::logicRateDiffers(144.0, 150.0));
        CHECK(minimum::logicRateDiffers(60.0, 240.0));
        CHECK(minimum::logicRateDiffers(60.0, 30.0));
        CHECK(minimum::logicRateDiffers(240.0, 60.0));
        // No data yet: never show a made up number.
        CHECK(!minimum::logicRateDiffers(0.0, 240.0));
        CHECK(!minimum::logicRateDiffers(60.0, 0.0));
    }

    void testPresets() {
        auto balanced = minimum::presetValues("Balanced");
        CHECK(balanced.known && balanced.particleCap == 256u && near(balanced.backgroundFps, 30.0) && !balanced.lowDetail);
        auto performance = minimum::presetValues("Performance");
        CHECK(performance.known && performance.particleCap == 128u && !performance.lowDetail);
        auto extreme = minimum::presetValues("Extreme");
        CHECK(extreme.known && extreme.particleCap == 48u && near(extreme.backgroundFps, 10.0) && !extreme.lowDetail);
        auto super = minimum::presetValues("Super Performance");
        CHECK(super.known && super.particleCap == 16u && near(super.backgroundFps, 5.0) && super.lowDetail);
        CHECK(!minimum::presetValues("Custom").known);
        CHECK(!minimum::presetValues("").known);
        CHECK(!minimum::presetValues("balanced").known);
        CHECK(!minimum::presetValues("Super").known);

        // Each step is stricter than the one before it.
        CHECK(balanced.particleCap > performance.particleCap);
        CHECK(performance.particleCap > extreme.particleCap);
        CHECK(extreme.particleCap > super.particleCap);
    }

    void testPopupPlan() {
        constexpr float kScreenHeight = 320.f;
        constexpr float kMaxHeight = kScreenHeight - 16.f;

        for (size_t toggles : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
            for (size_t presets : {0u, 1u, 3u, 4u, 5u, 6u}) {
                for (float maxHeight : {kMaxHeight, 280.f, 240.f}) {
                    auto plan = minimum::planPopup(maxHeight, toggles, presets);
                    CHECK(plan.width <= 427.f - 20.f);        // fits a 4:3 screen (427 wide)
                    CHECK(plan.height > 0.f);
                    CHECK(plan.toggles.size() == toggles);
                    CHECK(plan.presets.size() == presets);

                    std::vector<minimum::Box> all;
                    all.push_back(plan.live);
                    all.push_back(plan.settingsButton);
                    all.push_back(plan.credits);
                    for (auto const& b : plan.presets) all.push_back(b);
                    for (auto const& b : plan.toggles) all.push_back(b);

                    for (size_t i = 0; i < all.size(); ++i) {
                        CHECK(inside(all[i], plan.width, plan.height));
                        CHECK(all[i].w > 0.f && all[i].h > 0.f);
                        for (size_t j = i + 1; j < all.size(); ++j) {
                            CHECK(!overlap(all[i], all[j]));
                        }
                    }

                    // The title needs room above the live card.
                    CHECK(plan.height - plan.live.top() >= 28.f);
                }
            }
        }

        // The real case: 6 toggles and 5 presets must fit the 320 high game screen.
        auto real = minimum::planPopup(kMaxHeight, 6, 5);
        CHECK(real.height <= kMaxHeight);
        CHECK(real.presets.size() == 5);
        std::printf("popup plan: level %d, %.0f x %.0f\n", real.level, real.width, real.height);

        // Presets are in reading order: 3 in the first row, 2 (stretched) in the second.
        CHECK(real.presets[0].y == real.presets[1].y && real.presets[1].y == real.presets[2].y);
        CHECK(real.presets[3].y == real.presets[4].y);
        CHECK(real.presets[0].y > real.presets[3].y);
        CHECK(real.presets[0].x < real.presets[1].x && real.presets[3].x < real.presets[4].x);
        CHECK(real.presets[3].w > real.presets[0].w);
        // Both rows span the same width.
        CHECK(near(real.presets[2].right(), real.presets[4].right(), 0.01));
        CHECK(near(real.presets[0].x, real.presets[3].x, 0.01));

        // Preset buttons are wide enough to read "Performance" and "Super Performance".
        CHECK(real.presets[2].w >= 100.f);
        CHECK(real.presets[4].w >= 150.f);

        // A short screen switches to tighter spacing instead of overflowing.
        auto roomy = minimum::planPopup(1000.f, 6, 5);
        CHECK(roomy.level == 0);
        auto tight = minimum::planPopup(roomy.height - 5.f, 6, 5);
        CHECK(tight.level >= 1);
        CHECK(tight.height < roomy.height);

        // Toggle order: first card top left, second top right, same row.
        CHECK(real.toggles[0].y == real.toggles[1].y);
        CHECK(real.toggles[0].x < real.toggles[1].x);
        CHECK(real.toggles[0].y > real.toggles[2].y);

        // An odd last card is centered.
        auto odd = minimum::planPopup(kMaxHeight, 5, 5);
        CHECK(near(odd.toggles[4].x + odd.toggles[4].w / 2.0, odd.width / 2.0, 0.01));

        // The credits box has room for two lines.
        CHECK(real.credits.h >= 24.f);
    }

    void testFindFreeSpot() {
        using namespace minimum;
        Box area{0.f, 0.f, 400.f, 300.f};
        std::vector<Box> none;
        auto free1 = findFreeSpot({380.f, 280.f}, 36.f, none, area, -1.f, -1.f, 44.f, 8, 3, 4.f);
        CHECK(free1.free && near(free1.center.x, 380.0) && near(free1.center.y, 280.0));

        // Something sits on the preferred spot: the button slides along the edge.
        std::vector<Box> blocked{{360.f, 260.f, 40.f, 40.f}};
        auto free2 = findFreeSpot({380.f, 280.f}, 36.f, blocked, area, -1.f, -1.f, 44.f, 8, 3, 4.f);
        CHECK(free2.free);
        CHECK(!boxesCollide({free2.center.x - 18.f, free2.center.y - 18.f, 36.f, 36.f}, blocked[0], 4.f));

        // Everything is blocked: falls back to the preferred spot and says so.
        std::vector<Box> wall{{0.f, 0.f, 400.f, 300.f}};
        auto free3 = findFreeSpot({380.f, 280.f}, 36.f, wall, area, -1.f, -1.f, 44.f, 8, 3, 4.f);
        CHECK(!free3.free && near(free3.center.x, 380.0));
    }

    void testLatencyGate() {
        minimum::LatencyGate gate;
        double t = 0.0;

        // Steady 60 FPS: starts on the second steady window.
        bool on = false;
        for (int i = 0; i < 4; ++i) { on = gate.update(60.0, t, true); t += 0.5; }
        CHECK(on);

        // FPS falls clearly: switches off and stays off for 10 s even if FPS recovers.
        on = gate.update(40.0, t, true); t += 0.5;
        CHECK(!on);
        CHECK(gate.trips() == 1);
        for (int i = 0; i < 18; ++i) { on = gate.update(60.0, t, true); t += 0.5; }
        CHECK(!on);                       // 9 s later, still locked
        for (int i = 0; i < 6; ++i) { on = gate.update(60.0, t, true); t += 0.5; }
        CHECK(on);                        // lock expired

        // Second trip locks for 20 s.
        on = gate.update(30.0, t, true); t += 0.5;
        CHECK(!on && gate.trips() == 2);
        for (int i = 0; i < 38; ++i) { on = gate.update(60.0, t, true); t += 0.5; }
        CHECK(!on);
        for (int i = 0; i < 6; ++i) { on = gate.update(60.0, t, true); t += 0.5; }
        CHECK(on);

        // Not wanted: always off.
        CHECK(!gate.update(60.0, t, false));
    }

    void testLatencyDecay() {
        minimum::LatencyGate gate;
        double t = 0.0;
        auto step = [&](double fps) {
            bool const on = gate.update(fps, t, true);
            t += 0.5;
            return on;
        };

        for (int i = 0; i < 4; ++i) step(60.0);
        CHECK(gate.active());
        step(40.0);                                    // trip: one trip on record, locked
        CHECK(gate.trips() == 1);
        CHECK(!gate.active());

        int locked = 0;
        while (!gate.active() && locked < 200) {
            step(60.0);
            ++locked;
        }
        CHECK(locked * 0.5 == 10.0);                   // the first backoff is still 10 s

        for (int i = 0; i < 220; ++i) step(60.0);      // 110 s of Active, no trip
        CHECK(gate.trips() == 1);
        for (int i = 0; i < 20; ++i) step(60.0);       // 120 s of Active: forgiven
        CHECK(gate.trips() == 0);
        for (int i = 0; i < 1000; ++i) step(60.0);     // never below zero
        CHECK(gate.trips() == 0 && gate.active());

        step(40.0);                                    // a new trip is a first trip again
        CHECK(gate.trips() == 1 && !gate.active());
        locked = 0;
        while (!gate.active() && locked < 200) {
            step(60.0);
            ++locked;
        }
        CHECK(locked * 0.5 == 10.0);                   // 10 s, not 20 s
    }

    void testAdaptiveCap() {
        minimum::AdaptiveCapGovernor g;
        g.noteWindow(5, 5);                            // too few frames: ignored
        CHECK(g.step() == 0);
        g.noteWindow(60, 30);                          // one bad second does nothing
        CHECK(g.step() == 0);
        g.noteWindow(60, 30);
        CHECK(g.step() == 1);
        for (int i = 0; i < 10; ++i) g.noteWindow(60, 30);
        CHECK(g.step() == 3);                          // never deeper than 3
        for (int i = 0; i < 9; ++i) g.noteWindow(60, 0);
        CHECK(g.step() == 3);
        g.noteWindow(60, 0);                           // ten smooth seconds: one step back
        CHECK(g.step() == 2);
        g.reset();
        CHECK(g.step() == 0);
    }

    void testThermalGovernor() {
        minimum::ThermalGovernor g;
        CHECK(g.update(3, 0.0) == 1);                  // severe: one step at once
        CHECK(g.update(3, 4.9) == 1);                  // no second step before 5 s
        CHECK(g.update(3, 5.0) == 2);
        CHECK(g.update(4, 10.0) == 3);                 // critical: deepest step
        CHECK(g.update(6, 15.0) == 3);                 // shutdown does not go deeper
        CHECK(g.update(0, 16.0) == 3);                 // nominal starts at 16 s
        CHECK(g.update(0, 45.9) == 3);
        CHECK(g.update(0, 46.0) == 2);                 // 30 s nominal: one step back
        CHECK(g.update(1, 60.0) == 2);                 // light counts as nominal
        CHECK(g.update(0, 75.9) == 2);
        CHECK(g.update(0, 76.0) == 1);
        CHECK(g.update(0, 106.0) == 0);
        CHECK(g.update(0, 500.0) == 0);

        minimum::ThermalGovernor h;
        CHECK(h.update(3, 0.0) == 1);
        CHECK(h.update(2, 1.0) == 1);                  // moderate holds the step
        CHECK(h.update(0, 2.0) == 1);
        CHECK(h.update(0, 31.9) == 1);                 // 29.9 s nominal is not enough
        CHECK(h.update(0, 32.0) == 0);
        CHECK(h.update(3, 40.0) == 1);
        CHECK(h.update(-1, 41.0) == 1);                // error holds the step
    }

    void testCapSteps() {
        CHECK(minimum::cappedPoolSize(128, 0) == 128u);
        CHECK(minimum::cappedPoolSize(128, 1) == 64u);
        CHECK(minimum::cappedPoolSize(128, 2) == 32u);
        CHECK(minimum::cappedPoolSize(128, 3) == 16u);
        CHECK(minimum::cappedPoolSize(256, 3) == 32u);
        CHECK(minimum::cappedPoolSize(4, 3) == 4u);    // never raised above the configured cap
        CHECK(minimum::cappedPoolSize(20, 3) == 16u);
        CHECK(minimum::combinedCapStep(2, 2) == 3);    // adaptive + thermal, clamped
        CHECK(minimum::combinedCapStep(1, 0) == 1);
        CHECK(minimum::combinedCapStep(-1, 1) == 1);
        CHECK(minimum::combinedCapStep(0, 0) == 0);
    }

    void testSpawnBudget() {
        CHECK(minimum::budgetedPoolSize(128, 500, 0) == 128u);   // off
        CHECK(minimum::budgetedPoolSize(128, 0, 3) == 128u);
        CHECK(minimum::budgetedPoolSize(128, 2, 3) == 128u);     // third system still full
        CHECK(minimum::budgetedPoolSize(128, 3, 3) == minimum::kOverBudgetPool);
        CHECK(minimum::budgetedPoolSize(2, 9, 3) == 2u);         // never bigger than asked
    }

    void testCulling() {
        using namespace minimum;
        Box const screen{0.f, 0.f, 568.f, 320.f};

        ParticleBounds plain;                 // speed 10 for 0.5 s, 4 pt particles
        plain.speed = 10.f;
        plain.life = 0.5f;
        plain.maxSize = 4.f;
        CHECK(near(particleReach(plain), 5.0 + 4.0 + kCullMarginPt));   // v*t + size + margin

        ParticleBounds gravity;               // gravity 500 pt/s^2 for 2 s moves a particle 1000 pt
        gravity.life = 2.f;
        gravity.accel = 500.f;
        CHECK(near(particleReach(gravity), 0.5 * 500.0 * 4.0 + kCullMarginPt));

        ParticleBounds orbit;                 // radius mode: 30 pt orbit, 10 pt particles
        orbit.radius = 30.f;
        orbit.maxSize = 10.f;
        CHECK(near(particleReach(orbit), 30.0 + 10.0 + kCullMarginPt));

        ParticleBounds offset;                // emitter offset 12 pt, position variance 8 pt
        offset.sourceLength = 12.f;
        offset.posVar = 8.f;
        CHECK(near(particleReach(offset), 12.0 + 8.0 + kCullMarginPt));

        ParticleBounds negative;              // nonsense input never shrinks the reach below the margin
        negative.speed = -5.f;
        negative.life = -1.f;
        negative.accel = -3.f;
        negative.maxSize = -3.f;
        CHECK(near(particleReach(negative), kCullMarginPt));
        CHECK(particleReach(gravity) > particleReach(plain));

        CHECK(squareTouches({600.f, 160.f}, 40.f, screen));   // edge exactly reaches the screen
        CHECK(!squareTouches({620.f, 160.f}, 40.f, screen));  // fully to the right
        CHECK(squareTouches({-10.f, -10.f}, 20.f, screen));   // corner overlap
        CHECK(!squareTouches({-100.f, 160.f}, 40.f, screen)); // fully to the left
        CHECK(!squareTouches({300.f, 400.f}, 40.f, screen));  // fully above
    }

    void testRatesAndSlowFrames() {
        using namespace minimum;
        CHECK(!isSlowFrame(1.0 / 60.0, 60.0));
        CHECK(!isSlowFrame(0.0249, 60.0));          // just under 1.5 x the 60 fps frame time
        CHECK(isSlowFrame(0.0251, 60.0));           // just over it
        CHECK(isSlowFrame(2.0 / 60.0, 60.0));       // a dropped frame is slow
        CHECK(!isSlowFrame(0.0124, 120.0));         // 120 fps: 12.5 ms is the line
        CHECK(isSlowFrame(0.0126, 120.0));
        CHECK(!isSlowFrame(0.02, 0.0));             // no target given: 60 assumed, 20 ms is fine

        CHECK(perSecond(30, 0.5) == 60u);
        CHECK(perSecond(0, 0.5) == 0u);
        CHECK(perSecond(7, 0.0) == 0u);             // empty span: no division by zero
        CHECK(perSecond(10, 3.0) == 3u);            // 3.33 rounds down
        CHECK(perSecond(11, 3.0) == 4u);            // 3.67 rounds up
    }

    void testBottleneck() {
        using namespace minimum;
        // Needs a full window of frames first.
        CHECK(classifyBottleneck(16.0, 16.0, 59, true) == Bottleneck::None);
        CHECK(classifyBottleneck(17.0, 20.0, 60, false) == Bottleneck::CpuBound);  // share 0.85 exactly
        CHECK(classifyBottleneck(13.0, 16.0, 60, false) == Bottleneck::None);      // share 0.81
        CHECK(classifyBottleneck(4.8, 16.0, 60, true) == Bottleneck::GpuBound);    // share 0.3, sync on
        CHECK(classifyBottleneck(4.8, 16.0, 60, false) == Bottleneck::None);       // sync off: no GPU claim
        CHECK(classifyBottleneck(9.6, 16.0, 60, true) == Bottleneck::None);        // share 0.6
        CHECK(classifyBottleneck(0.0, 0.0, 60, true) == Bottleneck::None);
        CHECK(std::string(bottleneckLabel(Bottleneck::CpuBound)) == "CPU bound");
        CHECK(std::string(bottleneckLabel(Bottleneck::GpuBound)) == "GPU bound");
        CHECK(std::string(bottleneckLabel(Bottleneck::None)).empty());
    }

    void testFrameLimit() {
        double const sixty = 1.0 / 60.0;
        CHECK(near(minimum::frameWaitSeconds(0.004, sixty), sixty - 0.004));
        CHECK(near(minimum::frameWaitSeconds(0.1, sixty), 0.0));   // already late: no wait

        auto s = minimum::splitWait(0.0105, 0.001);
        CHECK(near(s.sleepSec, 0.0095) && near(s.spinSec, 0.001));
        CHECK(near(s.sleepSec + s.spinSec, 0.0105));
        auto shortWait = minimum::splitWait(0.0005, 0.001);
        CHECK(near(shortWait.sleepSec, 0.0) && near(shortWait.spinSec, 0.0005));
        auto none = minimum::splitWait(-1.0, 0.001);
        CHECK(near(none.sleepSec, 0.0) && near(none.spinSec, 0.0));
    }

    void testFrameRateUnlock() {
        double const sixty = 1.0 / 60.0;
        CHECK(near(minimum::unlockedAnimationInterval(sixty, false, 120), sixty));
        CHECK(near(minimum::unlockedAnimationInterval(sixty, true, 120), 1.0 / 120.0));
        CHECK(near(minimum::unlockedAnimationInterval(1.0 / 240.0, true, 120), 1.0 / 240.0));  // faster kept
        CHECK(near(minimum::unlockedAnimationInterval(1.0 / 20.0, true, 10), 1.0 / 30.0));     // limit clamped up to 30
        CHECK(near(minimum::unlockedAnimationInterval(sixty, true, 10), sixty));              // 60 is already faster than 30
        CHECK(near(minimum::unlockedAnimationInterval(sixty, true, 999), 1.0 / 240.0));        // clamped down
        CHECK(near(minimum::unlockedAnimationInterval(0.0, true, 120), 0.0));                  // no request yet
    }

    void testTapsAndRecorder() {
        minimum::TapStreak taps(5, 2.0);
        bool done = false;
        for (int i = 0; i < 5; ++i) done = taps.tap(0.1 * i);
        CHECK(done);

        minimum::TapStreak slow(5, 2.0);
        bool any = false;
        for (int i = 0; i < 5; ++i) any = any || slow.tap(0.6 * i);   // 2.4 s for five taps
        CHECK(!any);

        // 60 s of samples, 0.5 s apart (exact in binary, so the last one lands on 60.0).
        minimum::FrameRecorder rec;
        rec.start(100.0, 60.0);
        CHECK(rec.active());
        CHECK(near(rec.remainingSec(100.0), 60.0));
        bool finished = false;
        int count = 0;
        for (int i = 1; i <= 200 && !finished; ++i) {
            finished = rec.add(100.0 + 0.5 * i, 16.5, 4.0);
            ++count;
        }
        CHECK(finished);
        CHECK(!rec.active());
        CHECK(count == 120);
        CHECK(rec.size() == 120);
        CHECK(!rec.add(300.0, 1.0, 1.0));              // ignored after the end
        CHECK(rec.size() == 120);

        std::string const text = rec.csv();
        CHECK(text.rfind("elapsed_s,frame_ms,cpu_ms\n", 0) == 0);
        CHECK(std::count(text.begin(), text.end(), '\n') == 121);   // header + 120 rows
    }

    void testPopupFit() {
        for (float screen : {224.f, 240.f, 320.f}) {
            float const maxHeight = screen - 16.f;
            for (size_t toggles : {4u, 6u, 8u}) {
                auto p = minimum::planPopup(maxHeight, toggles, 5);
                CHECK(p.fitScale > 0.f && p.fitScale <= 1.f);
                CHECK(p.height * p.fitScale <= maxHeight + 0.01f);
                // The fitted popup still lays out without overlaps (checked unscaled).
                CHECK(p.presets.size() == 5u);
            }
        }
        auto tall = minimum::planPopup(304.f, 6, 5);   // 320 pt screen: no scaling needed
        CHECK(near(tall.fitScale, 1.0));
        auto short224 = minimum::planPopup(208.f, 6, 5);   // 224 pt screen
        CHECK(short224.fitScale < 1.f);
        CHECK(near(short224.height * short224.fitScale, 208.0, 0.01));
        std::printf("popup at 224 pt: level %d, height %.0f, fit %.3f\n",
                    short224.level, short224.height, short224.fitScale);
    }

}

    void testFrameGraph() {
        using minimum::FpsBand;
        // 60 FPS budget is 16.67 ms: up to 17.5 ms is Good, up to 25 ms is Okay.
        CHECK(minimum::classifyFrameMs(16.0, 60.0) == FpsBand::Good);
        CHECK(minimum::classifyFrameMs(17.5, 60.0) == FpsBand::Good);
        CHECK(minimum::classifyFrameMs(20.0, 60.0) == FpsBand::Okay);
        CHECK(minimum::classifyFrameMs(25.0, 60.0) == FpsBand::Okay);
        CHECK(minimum::classifyFrameMs(40.0, 60.0) == FpsBand::Bad);
        // A broken target falls back to 60.
        CHECK(minimum::classifyFrameMs(16.0, 0.0) == FpsBand::Good);

        // Ceiling: twice the budget, never below 8 ms.
        CHECK(near(minimum::sparkCeilingMs(60.0), 2000.0 / 60.0));
        CHECK(near(minimum::sparkCeilingMs(0.0), 2000.0 / 60.0));
        CHECK(near(minimum::sparkCeilingMs(240.0), 2000.0 / 240.0));
        CHECK(near(minimum::sparkCeilingMs(1000.0), 8.0));

        // A frame on budget is half height at 60 FPS.
        double const ceiling = minimum::sparkCeilingMs(60.0);
        CHECK(near(minimum::sparkBarFraction(1000.0 / 60.0, ceiling), 0.5f, 1e-4));
        CHECK(near(minimum::sparkBarFraction(100.0, ceiling), 1.0));
        CHECK(near(minimum::sparkBarFraction(0.0, ceiling), 0.0));
        CHECK(near(minimum::sparkBarFraction(-4.0, ceiling), 0.0));
        CHECK(near(minimum::sparkBarFraction(std::nan(""), ceiling), 0.0));
        CHECK(near(minimum::sparkBarFraction(10.0, 0.0), 0.0));
    }

int main() {
    testFrameStats();
    testBands();
    testFrameGraph();
    testLogicRate();
    testPresets();
    testPopupPlan();
    testPopupFit();
    testFindFreeSpot();
    testLatencyGate();
    testLatencyDecay();
    testAdaptiveCap();
    testThermalGovernor();
    testCapSteps();
    testSpawnBudget();
    testCulling();
    testRatesAndSlowFrames();
    testBottleneck();
    testFrameLimit();
    testFrameRateUnlock();
    testTapsAndRecorder();

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
