// Tests for include/minimum_logic.hpp. No Geode SDK needed, any C++20 compiler works:
//
//   g++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude tests/logic_test.cpp -o logic_test && ./logic_test

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

    void testPresets() {
        auto balanced = minimum::presetValues("Balanced");
        CHECK(balanced.known && balanced.particleCap == 256u && near(balanced.backgroundFps, 30.0));
        auto performance = minimum::presetValues("Performance");
        CHECK(performance.known && performance.particleCap == 128u);
        auto extreme = minimum::presetValues("Extreme");
        CHECK(extreme.known && extreme.particleCap == 48u && near(extreme.backgroundFps, 10.0));
        CHECK(!minimum::presetValues("Custom").known);
        CHECK(!minimum::presetValues("").known);
        CHECK(!minimum::presetValues("balanced").known);
    }

    void testPopupPlan() {
        for (size_t toggles : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
            for (float maxHeight : {310.f, 300.f, 280.f, 240.f}) {
                auto plan = minimum::planPopup(maxHeight, toggles);
                CHECK(plan.width <= 427.f - 20.f);        // fits a 4:3 screen (427 wide)
                CHECK(plan.height > 0.f);
                CHECK(plan.toggles.size() == toggles);
                CHECK(plan.presets.size() == 4);

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
                CHECK(plan.height - plan.live.top() >= 30.f);
            }
        }

        // The real case: 6 toggles on a 320 high screen must fit without squeezing.
        auto real = minimum::planPopup(310.f, 6);
        CHECK(real.height <= 310.f);
        CHECK(!real.compact);

        // A short screen switches to tighter spacing instead of overflowing.
        auto tight = minimum::planPopup(270.f, 6);
        CHECK(tight.compact);
        CHECK(tight.height < real.height);

        // Toggle order: first card top left, second top right, same row.
        CHECK(real.toggles[0].y == real.toggles[1].y);
        CHECK(real.toggles[0].x < real.toggles[1].x);
        CHECK(real.toggles[0].y > real.toggles[2].y);

        // An odd last card is centered.
        auto odd = minimum::planPopup(310.f, 5);
        CHECK(near(odd.toggles[4].x + odd.toggles[4].w / 2.0, odd.width / 2.0, 0.01));
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

}

int main() {
    testFrameStats();
    testBands();
    testPresets();
    testPopupPlan();
    testFindFreeSpot();
    testLatencyGate();

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
