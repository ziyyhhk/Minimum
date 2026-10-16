// Tests for include/minimum_logic.hpp. No Geode SDK needed, any C++20 compiler works:
//
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude tests/logic_test.cpp -o logic_test && ./logic_test

#include <cmath>
#include <cstdint>
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


    // ---- F2 / F14: popup fit scale -------------------------------------------------------

    void testPopupFit() {
        using namespace minimum;

        // Real case on a 320 pt tall screen (max height = screen - 16): fits without scaling.
        auto at320 = planPopup(320.f - 16.f, 6, 5);
        CHECK(near(at320.fitScale, 1.0));
        CHECK(at320.height <= 304.f);

        // Real case on a 224 pt tall screen: the tightest spacing is still too tall, so the
        // plan keeps level 2 and carries a fit scale. Scaled, it fits.
        auto at224 = planPopup(224.f - 16.f, 6, 5);
        CHECK(at224.level == 2);
        CHECK(at224.height > 208.f);
        CHECK(at224.fitScale < 1.f && at224.fitScale > 0.f);
        CHECK(near(at224.fitScale, 208.0 / at224.height, 1e-5));
        CHECK(at224.height * at224.fitScale <= 208.f + 0.01f);
        CHECK(at224.width * at224.fitScale <= at224.width);
        std::printf("popup plan at 224 pt: level %d, height %.0f, fit scale %.3f\n", at224.level, at224.height, at224.fitScale);

        // Every layout fits both screen heights, with the fit scale applied.
        for (float screen : {224.f, 240.f, 320.f}) {
            float const maxHeight = screen - 16.f;
            for (size_t toggles : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
                for (size_t presets : {0u, 1u, 3u, 4u, 5u, 6u}) {
                    auto plan = planPopup(maxHeight, toggles, presets);
                    CHECK(plan.fitScale > 0.f && plan.fitScale <= 1.f);
                    CHECK(plan.height * plan.fitScale <= maxHeight + 0.01f);
                    // A plan that already fits is never scaled.
                    if (plan.height <= maxHeight) CHECK(plan.fitScale == 1.f);
                }
            }
        }

        // A nonsense max height cannot produce a zero or negative scale.
        CHECK(planPopup(0.f, 6, 5).fitScale >= 0.1f);
        CHECK(planPopup(-50.f, 6, 5).fitScale >= 0.1f);

        // F14: the card pitch is exactly one card plus the 4 pt gap on every spacing level.
        for (float maxHeight : {1000.f, 304.f, 150.f}) {
            auto plan = planPopup(maxHeight, 6, 5);
            CHECK(near(plan.toggles[0].y - plan.toggles[2].y, plan.toggles[0].h + 4.f, 0.001));
        }
    }

    // ---- F5: latency trip decay ----------------------------------------------------------

    void testLatencyDecay() {
        // Steady 60 FPS windows of 0.5 s; helper that runs `seconds` of them.
        auto run = [](minimum::LatencyGate& g, double& t, double seconds, double fps) {
            bool on = false;
            for (double e = 0.0; e < seconds - 1e-9; e += 0.5) { on = g.update(fps, t, true); t += 0.5; }
            return on;
        };

        minimum::LatencyGate gate;
        double t = 0.0;
        run(gate, t, 2.0, 60.0);
        CHECK(gate.active());

        // Three trips in a row: locks of 10 s, 20 s, 40 s (the backoff is unchanged).
        for (int trip = 1; trip <= 3; ++trip) {
            gate.update(30.0, t, true); t += 0.5;
            CHECK(gate.trips() == trip);
            double const lock = 10.0 * std::pow(2.0, trip - 1);
            run(gate, t, lock - 1.0, 60.0);
            CHECK(!gate.active());                 // still locked just before the end
            run(gate, t, 3.0, 60.0);               // lock expires, then the gate needs steady windows
            CHECK(gate.active());
        }
        CHECK(gate.trips() == 3);

        // Time Active since the last trip is what counts. Everything above was short.
        // 119 s more: still 3 trips (the clean clock is at ~119 s plus the little before it).
        minimum::LatencyGate g2;
        double t2 = 0.0;
        run(g2, t2, 2.0, 60.0);
        g2.update(30.0, t2, true); t2 += 0.5;       // trip 1
        run(g2, t2, 12.0, 60.0);                    // lock + restart
        CHECK(g2.active());
        g2.update(30.0, t2, true); t2 += 0.5;       // trip 2 (wipes the clean clock)
        CHECK(g2.trips() == 2);
        run(g2, t2, 24.0, 60.0);
        CHECK(g2.active());

        // From here: exactly kDecaySeconds of clean Active time forgive one trip. The clock runs
        // from the moment the sync is Active again, so measure from there.
        double const clean0 = t2;
        double cleanSoFar = 0.0;
        while (g2.trips() == 2 && cleanSoFar < 400.0) {
            g2.update(60.0, t2, true); t2 += 0.5;
            cleanSoFar = t2 - clean0;
        }
        CHECK(g2.trips() == 1);
        // run(24 s) above already spent a few seconds Active after the lock, so the decay lands a
        // little before 120 s from clean0, and never more than the Active time spent so far early.
        CHECK(cleanSoFar >= 112.0 && cleanSoFar <= 120.5);

        // And another 120 s forgive the last one. Floor is 0.
        double const clean1 = t2;
        while (g2.trips() == 1 && (t2 - clean1) < 400.0) { g2.update(60.0, t2, true); t2 += 0.5; }
        CHECK(g2.trips() == 0);
        double const clean2 = t2;
        while ((t2 - clean2) < 600.0) { g2.update(60.0, t2, true); t2 += 0.5; }
        CHECK(g2.trips() == 0);                     // never negative
        CHECK(g2.active());

        // After decay the next trip locks for the SHORT time again (10 s), not 40 or 300.
        g2.update(30.0, t2, true); t2 += 0.5;
        CHECK(g2.trips() == 1);
        run(g2, t2, 9.0, 60.0);
        CHECK(!g2.active());
        run(g2, t2, 4.0, 60.0);
        CHECK(g2.active());

        // A trip restarts the clock: 100 s clean, trip, then 100 s clean is NOT enough for a
        // decay (3 trips -> 2 requires 120 s clean after the latest trip).
        minimum::LatencyGate g3;
        double t3 = 0.0;
        run(g3, t3, 2.0, 60.0);
        for (int i = 0; i < 2; ++i) { g3.update(30.0, t3, true); t3 += 0.5; run(g3, t3, 10.0 * std::pow(2.0, i) + 4.0, 60.0); }
        CHECK(g3.trips() == 2 && g3.active());
        run(g3, t3, 100.0, 60.0);
        CHECK(g3.trips() == 2);
        g3.update(30.0, t3, true); t3 += 0.5;       // trip 3 after 100 s clean
        CHECK(g3.trips() == 3);
        run(g3, t3, 40.0 + 4.0, 60.0);
        CHECK(g3.active());
        run(g3, t3, 100.0, 60.0);                   // only ~100 s clean since the trip
        CHECK(g3.trips() == 3);
        run(g3, t3, 30.0, 60.0);
        CHECK(g3.trips() == 2);


        // Exact timing on a fresh gate: after the trip the gate waits out its 10 s lock, becomes
        // Active again at a known time, and one trip is forgiven kDecaySeconds later.
        {
            minimum::LatencyGate g5;
            double t5 = 0.0;
            run(g5, t5, 2.0, 60.0);
            g5.update(30.0, t5, true); t5 += 0.5;            // trip, locked until t5 + 9.5
            CHECK(g5.trips() == 1);
            double activeAt = -1.0;
            while (activeAt < 0.0 && t5 < 100.0) {
                if (g5.update(60.0, t5, true)) activeAt = t5;
                t5 += 0.5;
            }
            CHECK(activeAt > 0.0);
            while (g5.trips() == 1 && t5 < 1000.0) {
                g5.update(60.0, t5, true);
                if (g5.trips() == 1) CHECK(t5 - activeAt < 120.0 + 1e-9);
                t5 += 0.5;
            }
            CHECK(g5.trips() == 0);
            // The update that forgave the trip is the one 120 s after the Active start (t5 was
            // advanced once more after it).
            CHECK(near((t5 - 0.5) - activeAt, 120.0, 0.51));
        }

        // Time spent locked out or not wanted does not count as clean Active time.
        minimum::LatencyGate g4;
        double t4 = 0.0;
        run(g4, t4, 2.0, 60.0);
        g4.update(30.0, t4, true); t4 += 0.5;
        CHECK(g4.trips() == 1);
        for (double e = 0.0; e < 600.0; e += 0.5) { g4.update(60.0, t4, false); t4 += 0.5; }
        CHECK(g4.trips() == 1);
    }

    // ---- F4: adaptive cap ----------------------------------------------------------------

    void testAdaptiveCap() {
        using namespace minimum;

        // capForStep: halves per step, floor 16, never above the configured cap.
        CHECK(capForStep(128, 0) == 128u);
        CHECK(capForStep(128, 1) == 64u);
        CHECK(capForStep(128, 2) == 32u);
        CHECK(capForStep(128, 3) == 16u);
        CHECK(capForStep(128, 9) == 16u);
        CHECK(capForStep(48, 3) == 16u);
        CHECK(capForStep(16, 3) == 16u);
        CHECK(capForStep(8, 2) == 8u);               // below the floor already: unchanged
        CHECK(capForStep(1000, -2) == 1000u);

        CHECK(combinedCapStep(0, 0) == 0);
        CHECK(combinedCapStep(2, 1) == 3);
        CHECK(combinedCapStep(3, 1) == kMaxCapStep);
        CHECK(combinedCapStep(-5, 0) == 0);

        AdaptiveCap cap;
        CHECK(cap.step() == 0);
        cap.update(60, 30);                           // 50 % slow: one bad second
        CHECK(cap.step() == 0);
        cap.update(60, 30);                           // two in a row: lower
        CHECK(cap.step() == 1);
        for (int i = 0; i < 20; ++i) cap.update(60, 60);
        CHECK(cap.step() == kMaxCapStep);             // never past the max

        // Too few frames to say anything: ignored, state untouched.
        AdaptiveCap few;
        for (int i = 0; i < 10; ++i) few.update(9, 9);
        CHECK(few.step() == 0);

        // A hitch alone changes nothing: bad, smooth, bad, smooth ...
        AdaptiveCap hitch;
        for (int i = 0; i < 20; ++i) { hitch.update(60, 40); hitch.update(60, 0); }
        CHECK(hitch.step() == 0);

        // The middle zone (2 % .. 25 % slow) resets the bad streak but does not raise.
        AdaptiveCap mid;
        mid.update(60, 40);                           // bad
        mid.update(60, 6);                            // 10 %: middle
        mid.update(60, 40);                           // bad again, streak was reset
        CHECK(mid.step() == 0);

        // Ten smooth seconds raise it by one step.
        AdaptiveCap rec;
        rec.update(60, 60); rec.update(60, 60);
        rec.update(60, 60); rec.update(60, 60);
        CHECK(rec.step() == 2);
        for (int i = 0; i < 9; ++i) rec.update(60, 0);
        CHECK(rec.step() == 2);
        rec.update(60, 0);
        CHECK(rec.step() == 1);
        for (int i = 0; i < 10; ++i) rec.update(60, 0);
        CHECK(rec.step() == 0);
        for (int i = 0; i < 50; ++i) rec.update(60, 0);
        CHECK(rec.step() == 0);                       // floor 0
        rec.update(60, 60); rec.update(60, 60);
        CHECK(rec.step() == 1);
        rec.reset();
        CHECK(rec.step() == 0);

        // F4: why only DRAWN frames are counted. 20 of the 60 drawn frames in a second are slow.
        // Counting the 180 logic-only frames of a 240 Hz logic loop as well dilutes that to
        // 20 / 240 = 8 %, in the "middle" zone, and the cap would never come down.
        AdaptiveCap drawnOnly, diluted;
        for (int i = 0; i < 10; ++i) {
            drawnOnly.update(60, 20);
            diluted.update(240, 20);
        }
        CHECK(drawnOnly.step() == kMaxCapStep);
        CHECK(diluted.step() == 0);

        // Slow frame threshold: 1.5x the target frame time ...
        CHECK(near(slowFrameThresholdMs(60.0, 0.0), 25.0, 1e-9));
        CHECK(near(slowFrameThresholdMs(120.0, 0.0), 12.5, 1e-9));
        CHECK(near(slowFrameThresholdMs(0.0, 0.0), 25.0, 1e-9));      // bad target: 60 FPS
        CHECK(isSlowFrame(26.0, 60.0, 0.0));
        CHECK(!isSlowFrame(24.0, 60.0, 0.0));
        // ... or 1.5x the gate's own interval when the gate draws less often than the target.
        CHECK(near(slowFrameThresholdMs(60.0, 1.0 / 30.0), 50.0, 1e-9));
        CHECK(!isSlowFrame(34.0, 60.0, 1.0 / 30.0));                 // a normal 30 FPS Draw Divide frame
        CHECK(isSlowFrame(60.0, 60.0, 1.0 / 30.0));
        // A gate faster than the target does not tighten the threshold.
        CHECK(near(slowFrameThresholdMs(30.0, 1.0 / 120.0), 50.0, 1e-9));
    }

    // ---- O4: thermal guard ---------------------------------------------------------------

    void testThermalGuard() {
        using namespace minimum;
        ThermalGuard g;
        CHECK(!g.throttling() && g.steps() == 0);

        g.update(0, 0.0);  g.update(1, 2.0);  g.update(2, 4.0);   // none, light, moderate: nothing
        CHECK(!g.throttling());

        g.update(3, 6.0);                                         // severe
        CHECK(g.throttling() && g.steps() == 1);
        g.update(4, 8.0);                                         // critical keeps it
        CHECK(g.throttling());

        // Recovery needs 30 s of nominal in a row.
        g.update(0, 10.0);
        CHECK(g.throttling());
        g.update(1, 39.9);
        CHECK(g.throttling());                                    // 29.9 s since nominal began
        g.update(0, 40.1);
        CHECK(!g.throttling() && g.steps() == 0);

        // Heat coming back in the middle of a recovery restarts the clock.
        g.update(3, 100.0);
        g.update(0, 102.0);
        g.update(3, 120.0);
        g.update(0, 125.0);
        g.update(0, 150.0);                                       // only 25 s nominal
        CHECK(g.throttling());
        g.update(0, 155.1);
        CHECK(!g.throttling());

        // Moderate heat while recovering neither recovers nor re-lowers, it restarts the clock.
        g.update(3, 200.0);
        g.update(0, 202.0);
        g.update(2, 220.0);
        g.update(0, 225.0);
        g.update(0, 240.0);                                       // 15 s since 225
        CHECK(g.throttling());
        g.update(1, 255.5);
        CHECK(!g.throttling());

        // An unknown status (-1) is ignored in both directions.
        g.update(3, 300.0);
        g.update(-1, 400.0);
        CHECK(g.throttling());
        g.update(0, 401.0);
        g.update(-1, 500.0);
        g.update(0, 431.5);
        CHECK(!g.throttling());
        g.update(-1, 600.0);
        CHECK(!g.throttling());

        g.update(5, 700.0);
        CHECK(g.throttling());
        g.reset();
        CHECK(!g.throttling());

        // The thermal step lowers the effective cap by exactly one step.
        CHECK(capForStep(128, combinedCapStep(0, 1)) == 64u);
        CHECK(capForStep(128, combinedCapStep(1, 1)) == 32u);
    }

    // ---- O2: bottleneck readout ----------------------------------------------------------

    void testBottleneck() {
        using namespace minimum;

        CHECK(classifyBottleneck(0.85, false) == Bottleneck::Cpu);
        CHECK(classifyBottleneck(0.99, true) == Bottleneck::Cpu);
        CHECK(classifyBottleneck(0.8499, false) == Bottleneck::None);
        CHECK(classifyBottleneck(0.8499, true) == Bottleneck::None);
        CHECK(classifyBottleneck(0.49, true) == Bottleneck::Gpu);
        CHECK(classifyBottleneck(0.49, false) == Bottleneck::None);   // needs the sync Active
        CHECK(classifyBottleneck(0.5, true) == Bottleneck::None);     // 0.5 itself is not "below"
        CHECK(classifyBottleneck(0.7, true) == Bottleneck::None);
        CHECK(classifyBottleneck(-1.0, true) == Bottleneck::None);

        CHECK(bottleneckLabel(Bottleneck::Cpu) == "CPU-bound");
        CHECK(bottleneckLabel(Bottleneck::Gpu) == "GPU-bound");
        CHECK(bottleneckLabel(Bottleneck::None).empty());

        FrameStats s;
        CHECK(near(s.cpuShare(60), -1.0));
        CHECK(bottleneckFor(s, true) == Bottleneck::None);

        for (int i = 0; i < 20; ++i) s.push(16.7, 15.0);
        CHECK(bottleneckFor(s, true) == Bottleneck::None);            // under 30 frames: no verdict

        for (int i = 0; i < 80; ++i) s.push(16.7, 15.0);              // 15 / 16.7 = 0.898
        CHECK(near(s.cpuShare(60), 15.0 / 16.7, 1e-4));
        CHECK(bottleneckFor(s, false) == Bottleneck::Cpu);

        // Only the newest 60 frames count: 60 GPU-heavy frames push the CPU-heavy ones out.
        for (int i = 0; i < 60; ++i) s.push(16.7, 5.0);
        CHECK(near(s.cpuShare(60), 5.0 / 16.7, 1e-4));
        CHECK(bottleneckFor(s, true) == Bottleneck::Gpu);
        CHECK(bottleneckFor(s, false) == Bottleneck::None);

        // The share is a ratio of sums, not an average of ratios.
        FrameStats w;
        for (int i = 0; i < 30; ++i) w.push(10.0, 10.0);
        for (int i = 0; i < 30; ++i) w.push(30.0, 0.0);
        CHECK(near(w.cpuShare(60), 300.0 / 1200.0, 1e-6));

        // Clamped to 0..1 (cpu time can include a little more than the frame start-to-start).
        FrameStats c;
        for (int i = 0; i < 40; ++i) c.push(10.0, 12.0);
        CHECK(near(c.cpuShare(60), 1.0));
        CHECK(bottleneckFor(c, false) == Bottleneck::Cpu);
        FrameStats z;
        for (int i = 0; i < 40; ++i) z.push(0.0, 0.0);
        CHECK(near(z.cpuShare(60), -1.0));
    }

    // ---- O1: off-screen particle culling -------------------------------------------------

    void testCulling() {
        using namespace minimum;
        Box const screen{0.f, 0.f, 480.f, 320.f};
        Affine const identity;

        auto boundsOf = [](float x0, float y0, float x1, float y1) {
            BoundsAccumulator b;
            b.add(x0, y0);
            b.add(x1, y1);
            return b;
        };

        // The accumulator.
        BoundsAccumulator empty;
        CHECK(!empty.valid());
        BoundsAccumulator one;
        one.add(5.f, -3.f);
        CHECK(one.valid() && near(one.box().x, 5.0) && near(one.box().w, 0.0) && near(one.box().h, 0.0));
        auto b = boundsOf(10.f, 20.f, -4.f, 50.f);
        CHECK(b.valid());
        CHECK(near(b.box().x, -4.0) && near(b.box().y, 20.0) && near(b.box().w, 14.0) && near(b.box().h, 30.0));
        BoundsAccumulator nan;
        nan.add(1.f, 1.f);
        nan.add(std::nanf(""), 2.f);
        CHECK(!nan.valid());
        BoundsAccumulator inf;
        inf.add(1.f, 1.f);
        inf.add(2.f, INFINITY);
        CHECK(!inf.valid());

        // Transforms.
        Affine const move{1.f, 0.f, 0.f, 1.f, 100.f, 50.f};
        Box const moved = transformBox({0.f, 0.f, 10.f, 20.f}, move);
        CHECK(near(moved.x, 100.0) && near(moved.y, 50.0) && near(moved.w, 10.0) && near(moved.h, 20.0));
        Affine const scale{2.f, 0.f, 0.f, 3.f, 0.f, 0.f};
        Box const scaled = transformBox({1.f, 1.f, 10.f, 10.f}, scale);
        CHECK(near(scaled.x, 2.0) && near(scaled.y, 3.0) && near(scaled.w, 20.0) && near(scaled.h, 30.0));
        // 90 degrees counter clockwise: (x, y) -> (-y, x). The box of a 10 x 4 rectangle becomes 4 x 10.
        Affine const rot90{0.f, 1.f, -1.f, 0.f, 0.f, 0.f};
        Box const rotated = transformBox({0.f, 0.f, 10.f, 4.f}, rot90);
        CHECK(near(rotated.x, -4.0) && near(rotated.y, 0.0) && near(rotated.w, 4.0) && near(rotated.h, 10.0));
        // Flipped (negative scale) still gives a normal box with positive size.
        Box const flipped = transformBox({0.f, 0.f, 10.f, 10.f}, Affine{-1.f, 0.f, 0.f, 1.f, 0.f, 0.f});
        CHECK(flipped.w > 0.f && near(flipped.x, -10.0));

        // Building the transform from three converted points gives the same transform.
        Affine const rebuilt = affineFromUnitPoints(
            applyAffine(rot90, {0.f, 0.f}), applyAffine(rot90, {1.f, 0.f}), applyAffine(rot90, {0.f, 1.f})
        );
        CHECK(near(rebuilt.a, rot90.a) && near(rebuilt.b, rot90.b) && near(rebuilt.c, rot90.c) && near(rebuilt.d, rot90.d));
        Affine const rebuiltMove = affineFromUnitPoints({100.f, 50.f}, {101.f, 50.f}, {100.f, 51.f});
        CHECK(near(rebuiltMove.a, 1.0) && near(rebuiltMove.d, 1.0) && near(rebuiltMove.tx, 100.0) && near(rebuiltMove.ty, 50.0));
        Vec2 const p = applyAffine(rebuilt, {3.f, 7.f});
        Vec2 const q = applyAffine(rot90, {3.f, 7.f});
        CHECK(near(p.x, q.x) && near(p.y, q.y));

        // Margin: the biggest size the system can reach, plus the fixed extra.
        CHECK(near(particleCullMargin(10.f, 5.f, 40.f, 10.f), 50.f + kCullExtraMargin));
        CHECK(near(particleCullMargin(60.f, 0.f, 20.f, 0.f), 60.f + kCullExtraMargin));
        CHECK(near(particleCullMargin(-8.f, 0.f, 0.f, 0.f), 8.f + kCullExtraMargin));          // negative = size too
        CHECK(near(particleCullMargin(std::nanf(""), 0.f, 12.f, 0.f), 12.f + kCullExtraMargin));
        CHECK(particleCullMargin(0.f, 0.f, 0.f, 0.f) >= kCullExtraMargin);

        float const margin = 40.f;

        // Clearly on screen, partly on screen, touching: not culled.
        CHECK(!shouldCullParticles(true, boundsOf(100.f, 100.f, 200.f, 200.f), identity, screen, margin));
        CHECK(!shouldCullParticles(true, boundsOf(-50.f, 100.f, 30.f, 200.f), identity, screen, margin));   // straddles the left edge
        CHECK(!shouldCullParticles(true, boundsOf(470.f, 300.f, 600.f, 400.f), identity, screen, margin));  // corner
        // Fully off screen, further away than the margin: culled, in every direction.
        CHECK(shouldCullParticles(true, boundsOf(-300.f, 100.f, -100.f, 200.f), identity, screen, margin));  // left
        CHECK(shouldCullParticles(true, boundsOf(600.f, 100.f, 700.f, 200.f), identity, screen, margin));    // right
        CHECK(shouldCullParticles(true, boundsOf(100.f, -300.f, 200.f, -100.f), identity, screen, margin));  // below
        CHECK(shouldCullParticles(true, boundsOf(100.f, 400.f, 200.f, 500.f), identity, screen, margin));    // above
        // Just off screen but inside the margin: kept (this is what the margin is for).
        CHECK(!shouldCullParticles(true, boundsOf(-60.f, 100.f, -20.f, 200.f), identity, screen, margin));
        CHECK(!shouldCullParticles(true, boundsOf(500.f, 100.f, 560.f, 200.f), identity, screen, margin));
        // Exactly at the margin edge: kept (borderline cases are drawn, see kCullEpsilon).
        CHECK(!shouldCullParticles(true, boundsOf(-100.f, 100.f, -40.f, 200.f), identity, screen, margin));
        CHECK(shouldCullParticles(true, boundsOf(-100.f, 100.f, -40.5f, 200.f), identity, screen, margin));

        // The local bounds are moved by the node's transform before the test.
        CHECK(shouldCullParticles(true, boundsOf(0.f, 0.f, 50.f, 50.f), Affine{1.f, 0.f, 0.f, 1.f, 2000.f, 0.f}, screen, margin));
        CHECK(!shouldCullParticles(true, boundsOf(1990.f, 0.f, 2040.f, 50.f), Affine{1.f, 0.f, 0.f, 1.f, -1900.f, 100.f}, screen, margin));
        // A camera zoom out (scale 0.5) pulls a far particle on screen.
        CHECK(!shouldCullParticles(true, boundsOf(700.f, 100.f, 800.f, 200.f), Affine{0.5f, 0.f, 0.f, 0.5f, 0.f, 0.f}, screen, margin));
        CHECK(shouldCullParticles(true, boundsOf(700.f, 100.f, 800.f, 200.f), identity, screen, margin));
        // Rotation: a thin tall box rotated onto the screen.
        {
            // Transform (x, y) -> (-y + 500, x). Local (0..10, 600..700) lands at x' = 500 - y =
            // -200..-100, y' = 0..10: off screen to the left.
            CHECK(shouldCullParticles(true, boundsOf(0.f, 600.f, 10.f, 700.f), Affine{0.f, 1.f, -1.f, 0.f, 500.f, 0.f}, screen, margin));
            // local (0..10, 100..200) lands at x' = 300..400, y' = 0..10: on screen.
            CHECK(!shouldCullParticles(true, boundsOf(0.f, 100.f, 10.f, 200.f), Affine{0.f, 1.f, -1.f, 0.f, 500.f, 0.f}, screen, margin));
        }

        // Never culls when it is switched off, when there is nothing to measure, or when the
        // numbers can not be trusted: a particle must never disappear because of bad data.
        CHECK(!shouldCullParticles(false, boundsOf(-300.f, 100.f, -100.f, 200.f), identity, screen, margin));
        CHECK(!shouldCullParticles(true, empty, identity, screen, margin));
        CHECK(!shouldCullParticles(true, nan, identity, screen, margin));
        Affine const bad{std::nanf(""), 0.f, 0.f, 1.f, 0.f, 0.f};
        CHECK(!shouldCullParticles(true, boundsOf(-300.f, 100.f, -100.f, 200.f), bad, screen, margin));
        Affine const huge{1e30f, 0.f, 0.f, 1e30f, 0.f, 0.f};
        CHECK(!shouldCullParticles(true, boundsOf(1e30f, 0.f, 3e38f, 1.f), huge, screen, margin));   // overflows to inf: draw

        // Zero size bounds (one point) work like any other box.
        BoundsAccumulator point;
        point.add(-500.f, 100.f);
        CHECK(shouldCullParticles(true, point, identity, screen, margin));
        BoundsAccumulator pointIn;
        pointIn.add(240.f, 160.f);
        CHECK(!shouldCullParticles(true, pointIn, identity, screen, margin));

        // Culling the draw never touches update(): the decision is a pure function of geometry,
        // it has no state, so the same input always gives the same answer.
        for (int i = 0; i < 5; ++i) {
            CHECK(shouldCullParticles(true, boundsOf(-300.f, 100.f, -100.f, 200.f), identity, screen, margin));
        }
    }

    // ---- O6: spawn budget ----------------------------------------------------------------

    void testSpawnBudget() {
        using namespace minimum;
        CHECK(spawnBudgetTotal(500, 0, 0) == 500u);          // 0 = off, always
        CHECK(spawnBudgetTotal(500, 99999, 0) == 500u);
        CHECK(spawnBudgetTotal(500, 99999, -3) == 500u);
        CHECK(spawnBudgetTotal(500, 0, 3) == 500u);          // 1st system of the frame
        CHECK(spawnBudgetTotal(500, 2, 3) == 500u);          // 3rd: still inside the budget
        CHECK(spawnBudgetTotal(500, 3, 3) == kOverBudgetPoolSize);   // 4th: over
        CHECK(spawnBudgetTotal(500, 50, 3) == kOverBudgetPoolSize);
        CHECK(spawnBudgetTotal(2, 50, 3) == 2u);             // never raises a small request
        CHECK(spawnBudgetTotal(0, 50, 3) == 0u);
        CHECK(spawnBudgetTotal(500, 0, 1) == 500u);
        CHECK(spawnBudgetTotal(500, 1, 1) == kOverBudgetPoolSize);
        CHECK(kOverBudgetPoolSize >= 1u);                    // a system is never created with 0 particles

        // The budget stays off while a level loads and for a moment after (grace frames).
        CHECK(!spawnBudgetActive(0));
        CHECK(!spawnBudgetActive(kSpawnBudgetGraceFrames - 1));
        CHECK(spawnBudgetActive(kSpawnBudgetGraceFrames));
        CHECK(spawnBudgetActive(100000));
    }

    // ---- O5: frame rate unlock -----------------------------------------------------------

    void testFpsUnlock() {
        using namespace minimum;
        CHECK(near(unlockedInterval(1.0 / 60.0, false, 120), 1.0 / 60.0));   // off: unchanged
        CHECK(near(unlockedInterval(1.0 / 60.0, true, 120), 1.0 / 120.0));
        CHECK(near(unlockedInterval(1.0 / 60.0, true, 240), 1.0 / 240.0));
        CHECK(near(unlockedInterval(1.0 / 60.0, true, 30), 1.0 / 60.0));     // limit below the request: the faster request stays
        CHECK(near(unlockedInterval(1.0 / 240.0, true, 120), 1.0 / 240.0));  // game already asks for more
        CHECK(near(unlockedInterval(1.0 / 60.0, true, 10), 1.0 / 60.0));     // limit clamped up to 30
        CHECK(near(unlockedInterval(1.0 / 60.0, true, 9999), 1.0 / 240.0));  // limit clamped down to 240
        CHECK(near(unlockedInterval(0.0, true, 120), 0.0));                  // 0 = no interval, untouched
        CHECK(near(unlockedInterval(-1.0, true, 120), -1.0));
        CHECK(near(unlockedInterval(1.0 / 15.0, true, 120), 1.0 / 120.0));
        CHECK(kFpsLimitMin == 30 && kFpsLimitMax == 240);
    }

    // ---- O7: precise limiter -------------------------------------------------------------

    void testPreciseLimiter() {
        using namespace minimum;

        // Only when the game's own cap is off (or well above the target).
        CHECK(!shouldPreciseLimit(false, 60.0, 0.0));
        CHECK(shouldPreciseLimit(true, 60.0, 0.0));                   // interval 0: no cap
        CHECK(shouldPreciseLimit(true, 60.0, 1.0 / 240.0));           // cap far above the target
        CHECK(!shouldPreciseLimit(true, 60.0, 1.0 / 60.0));           // cap exactly at the target
        CHECK(!shouldPreciseLimit(true, 60.0, 1.0 / 30.0));           // cap below the target
        CHECK(!shouldPreciseLimit(true, 60.0, 1.0 / 62.0));           // within 10 %: already capped
        CHECK(!shouldPreciseLimit(true, 0.0, 0.0));
        CHECK(!shouldPreciseLimit(true, 60.0, std::nan("")));

        // Wait plan: sleep until the last millisecond, spin the rest.
        auto plan = planWait(10.000, 10.010, 0.001);
        CHECK(near(plan.sleepSeconds, 0.009) && near(plan.spinSeconds, 0.001));
        auto shortPlan = planWait(10.000, 10.0004, 0.001);
        CHECK(near(shortPlan.sleepSeconds, 0.0) && near(shortPlan.spinSeconds, 0.0004));
        auto late = planWait(10.010, 10.000, 0.001);
        CHECK(late.sleepSeconds == 0.0 && late.spinSeconds == 0.0);
        auto exact = planWait(10.0, 10.0, 0.001);
        CHECK(exact.sleepSeconds == 0.0 && exact.spinSeconds == 0.0);
        auto noSpin = planWait(0.0, 0.01, 0.0);
        CHECK(near(noSpin.sleepSeconds, 0.01) && noSpin.spinSeconds == 0.0);
        auto negSpin = planWait(0.0, 0.01, -1.0);
        CHECK(near(negSpin.sleepSeconds, 0.01) && negSpin.spinSeconds == 0.0);

        // Deadlines: exact pacing over time, no burst after a late frame.
        double const interval = 1.0 / 60.0;
        double now = 100.0;
        double deadline = 0.0;                                        // nothing yet
        deadline = nextDeadline(deadline, now, interval);
        CHECK(near(deadline, now));                                   // first frame: no wait
        double work[] = {0.004, 0.005, 0.004, 0.030, 0.004, 0.004, 0.004};   // the 4th frame runs long
        double starts[8] = {};
        starts[0] = deadline;
        int n = 1;
        for (double w : work) {
            now = deadline + w;                                       // the frame's work ends here
            double const next = nextDeadline(deadline, now, interval);
            auto wait = planWait(now, next, 0.001);
            now += wait.sleepSeconds + wait.spinSeconds;              // after the wait we are AT the deadline
            deadline = next;
            starts[n++] = now;
        }
        // Normal frames start exactly one interval apart.
        CHECK(near(starts[1] - starts[0], interval, 1e-9));
        CHECK(near(starts[2] - starts[1], interval, 1e-9));
        CHECK(near(starts[3] - starts[2], interval, 1e-9));
        // The long frame (30 ms) is late, the next frame starts right away (no wait) ...
        CHECK(starts[4] - starts[3] >= 0.030 - 1e-9);
        // ... and afterwards the pace is exactly one interval again, not a burst of fast frames.
        CHECK(near(starts[6] - starts[5], interval, 1e-9));
        CHECK(near(starts[7] - starts[6], interval, 1e-9));
        for (int i = 1; i < 8; ++i) CHECK(starts[i] - starts[i - 1] >= interval - 1e-9 || i == 5);
    }

    // ---- HUD fit -------------------------------------------------------------------------

    void testHudFit() {
        using namespace minimum;
        // Fits: unchanged.
        CHECK(near(hudFitScale(0.5f, 100.f, 20.f, 500.f, 300.f), 0.5));
        CHECK(near(hudFitScale(2.0f, 100.f, 20.f, 500.f, 300.f), 2.0));
        // Too wide at HUD Scale 2.0: shrinks to exactly the room (600 * s = 500).
        CHECK(near(hudFitScale(2.0f, 600.f, 20.f, 500.f, 300.f), 500.0 / 600.0, 1e-5));
        // Too tall: the height decides.
        CHECK(near(hudFitScale(2.0f, 100.f, 400.f, 500.f, 300.f), 0.75, 1e-5));
        // Both too big: the smaller result wins, and the result really fits.
        float const both = hudFitScale(2.0f, 800.f, 200.f, 500.f, 300.f);
        CHECK(800.f * both <= 500.f + 0.01f && 200.f * both <= 300.f + 0.01f);
        // Never raised above what was asked for.
        CHECK(hudFitScale(0.5f, 10.f, 10.f, 500.f, 300.f) <= 0.5f);
        // Floor 0.1 for absurd room; a requested scale below the floor stays as requested.
        CHECK(near(hudFitScale(2.0f, 5000.f, 20.f, 10.f, 300.f), 0.1, 1e-6));
        CHECK(near(hudFitScale(0.05f, 5000.f, 20.f, 10.f, 300.f), 0.05, 1e-6));
        // Nothing measured yet (empty label) or no room information: do not touch the scale.
        CHECK(near(hudFitScale(1.5f, 0.f, 0.f, 500.f, 300.f), 1.5));
        CHECK(near(hudFitScale(1.5f, 600.f, 20.f, 0.f, 0.f), 1.5));
    }

    // ---- O3: rates -----------------------------------------------------------------------

    void testRates() {
        using namespace minimum;
        CHECK(ratePerSecond(0, 0.5) == 0u);
        CHECK(ratePerSecond(50, 0.5) == 100u);
        CHECK(ratePerSecond(1, 0.5) == 2u);
        CHECK(ratePerSecond(7, 2.0) == 4u);          // 3.5 rounds up
        CHECK(ratePerSecond(10, 0.0) == 0u);         // no time passed: no rate, no division by zero
        CHECK(ratePerSecond(10, -1.0) == 0u);
        CHECK(ratePerSecond(UINT64_MAX, 0.5) == 4000000000u);   // saturates instead of wrapping
    }

    // ---- O8: benchmark -------------------------------------------------------------------

    void testBenchmark() {
        using namespace minimum;

        BenchmarkRecorder rec;
        CHECK(!rec.active() && !rec.push(5.0, 16.7));            // inactive: records nothing
        CHECK(rec.samples() == 0);

        rec.start(1000.0);
        CHECK(rec.active());
        int finished = 0;
        double t = 1000.0;
        int frames = 0;
        while (rec.active() && frames < 100000) {
            t += 1.0 / 60.0;
            if (rec.push(t, 1000.0 / 60.0)) ++finished;
            ++frames;
        }
        CHECK(finished == 1);                                    // reports "finished" exactly once
        CHECK(!rec.active());
        CHECK(rec.samples() >= 3590 && rec.samples() <= 3601);   // ~60 s of 60 FPS
        CHECK(!rec.push(t + 1.0, 10.0));                         // nothing after the end
        CHECK(near(rec.averageFrameMs(), 1000.0 / 60.0, 1e-6));

        auto csv = rec.toCsv();
        CHECK(csv.rfind("frame,time_s,frame_ms\n", 0) == 0);     // header first
        size_t lines = 0;
        for (char c : csv) if (c == '\n') ++lines;
        CHECK(lines == rec.samples() + 1);
        CHECK(csv.find("\n0,0.0167,16.6667\n") != std::string::npos);

        // Restarting clears the old run.
        rec.start(5.0);
        CHECK(rec.active() && rec.samples() == 0);
        rec.push(5.5, 8.0);
        rec.push(6.0, 9.0);
        CHECK(rec.samples() == 2);
        CHECK(near(rec.elapsed(6.0), 1.0));
        rec.cancel();
        CHECK(!rec.active() && near(rec.elapsed(7.0), 0.0));
        CHECK(rec.toCsv() == "frame,time_s,frame_ms\n0,0.5000,8.0000\n1,1.0000,9.0000\n");

        // A run with no frames is still a valid CSV (just the header).
        BenchmarkRecorder emptyRun;
        emptyRun.start(0.0);
        CHECK(emptyRun.push(61.0, 1.0));
        CHECK(emptyRun.toCsv() == "frame,time_s,frame_ms\n");

        // Tap counter: 5 taps inside 3 s.
        TapCounter taps;
        CHECK(!taps.tap(0.0) && !taps.tap(0.5) && !taps.tap(1.0) && !taps.tap(1.5));
        CHECK(taps.tap(2.0));                                    // 5th within 3 s
        CHECK(!taps.tap(2.1));                                   // starts over
        TapCounter slow;
        for (int i = 0; i < 20; ++i) CHECK(!slow.tap(i * 1.0));  // one tap per second never reaches 5 in 3 s
        TapCounter edge;
        CHECK(!edge.tap(0.0) && !edge.tap(1.0) && !edge.tap(2.0) && !edge.tap(3.0));
        CHECK(edge.tap(3.0));                                    // exactly 3.0 s apart still counts
        TapCounter late;
        CHECK(!late.tap(0.0) && !late.tap(0.1) && !late.tap(0.2) && !late.tap(0.3));
        CHECK(!late.tap(3.5));                                   // the first four expired
        TapCounter two(2, 1.0);
        CHECK(!two.tap(0.0) && two.tap(0.5));
    }

}

int main() {
    testFrameStats();
    testBands();
    testLogicRate();
    testPresets();
    testPopupPlan();
    testFindFreeSpot();
    testLatencyGate();
    testPopupFit();
    testLatencyDecay();
    testAdaptiveCap();
    testThermalGuard();
    testBottleneck();
    testCulling();
    testSpawnBudget();
    testFpsUnlock();
    testPreciseLimiter();
    testHudFit();
    testRates();
    testBenchmark();

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
