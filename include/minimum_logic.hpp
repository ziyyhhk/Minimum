#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Minimum: pure logic, no Geode / cocos types.
//
// Kept separate from the game-facing code on purpose: everything in here can be compiled and
// tested with a plain C++ compiler, no Geode SDK needed.

namespace minimum {

    // ---------------------------------------------------------------------------------------
    // Button placement (pause menu)
    // ---------------------------------------------------------------------------------------

    struct Vec2 {
        float x = 0.f;
        float y = 0.f;
    };

    // Axis aligned box, (x, y) is the lower left corner.
    struct Box {
        float x = 0.f;
        float y = 0.f;
        float w = 0.f;
        float h = 0.f;

        float right() const { return x + w; }
        float top() const { return y + h; }
    };

    // True when the two boxes overlap or are closer to each other than `gap`.
    inline bool boxesCollide(Box const& a, Box const& b, float gap) {
        return a.x < b.right() + gap && b.x < a.right() + gap
            && a.y < b.top() + gap && b.y < a.top() + gap;
    }

    struct Spot {
        Vec2 center;
        // False when every candidate was blocked: `center` is then the preferred position.
        bool free = true;
    };

    // Finds a place for a square button of side `size`.
    //
    // Starts at `preferred` (the button's center, normally a corner) and steps away from the
    // corner: `dirX` / `dirY` are -1 or +1 and point towards the middle of the screen.
    // Candidates are tried closest-first, sliding along the screen edge before moving inward.
    // A candidate is accepted when its box stays inside `area` and keeps at least `gap`
    // distance from every blocker.
    inline Spot findFreeSpot(
        Vec2 preferred, float size, std::vector<Box> const& blockers, Box const& area,
        float dirX, float dirY, float step, int cols, int rows, float gap
    ) {
        struct Candidate {
            int col;
            int row;
        };
        std::vector<Candidate> candidates;
        candidates.reserve(static_cast<size_t>(std::max(cols, 0) * std::max(rows, 0)));
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                candidates.push_back({col, row});
            }
        }
        // One step inward costs as much as two steps along the edge. Stable, so equal cost
        // keeps the row-major order above and the result is deterministic.
        std::stable_sort(candidates.begin(), candidates.end(), [](Candidate const& a, Candidate const& b) {
            return (a.col + 2 * a.row) < (b.col + 2 * b.row);
        });

        for (auto const& c : candidates) {
            Vec2 const center{
                preferred.x + dirX * step * static_cast<float>(c.col),
                preferred.y + dirY * step * static_cast<float>(c.row),
            };
            Box const box{center.x - size / 2.f, center.y - size / 2.f, size, size};

            bool const inside = box.x >= area.x && box.right() <= area.right()
                             && box.y >= area.y && box.top() <= area.top();
            if (!inside) continue;

            bool blocked = false;
            for (auto const& other : blockers) {
                if (boxesCollide(box, other, gap)) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) return {center, true};
        }
        return {preferred, false};
    }

    // ---------------------------------------------------------------------------------------
    // Low latency guard
    // ---------------------------------------------------------------------------------------

    // Decides when the hard GPU sync (glFinish) may run.
    //
    // The sync lowers input latency but removes the overlap between CPU and GPU work, so on a
    // GPU bound machine it costs frame rate. The guard switches it off when it sees the frame
    // rate fall, and (this is the important part) it does NOT switch it straight back on:
    // every time it has to back off, it stays off twice as long (10 s, 20 s, 40 s ... max 5 min).
    //
    // The previous version re-enabled the sync one measurement later. The sync made the FPS
    // drop, the guard turned it off, the FPS recovered, the guard turned it on again, and so on,
    // so the frame rate alternated every half second.
    //
    // Call update() once per measurement window (about every 0.5 s).
    class LatencyGate {
    public:
        // fps    : frames drawn per second in the window that just ended.
        // now    : seconds on any monotonic clock.
        // wanted : the user wants the sync and the game is in a state where it may run.
        bool update(double fps, double now, bool wanted) {
            // Recent best frame rate, decays about 3 % per window so it follows the level.
            m_peak = std::max(fps, m_peak * 0.97);

            if (!wanted || now < m_lockedUntil) {
                m_on = false;
                return false;
            }

            if (m_on) {
                // The sync is running and the frame rate fell clearly below the recent best.
                if (fps < 30.0 || fps < 0.85 * m_peak) {
                    m_on = false;
                    ++m_trips;
                    double const lock = std::min(300.0, 10.0 * std::pow(2.0, m_trips - 1));
                    m_lockedUntil = now + lock;
                }
            }
            else if (fps >= 30.0 && fps >= 0.97 * m_peak) {
                // Only start while the game is holding a steady frame rate.
                m_on = true;
            }
            return m_on;
        }

        bool active() const { return m_on; }
        int trips() const { return m_trips; }

    private:
        double m_peak = 0.0;
        double m_lockedUntil = 0.0;
        int m_trips = 0;
        bool m_on = false;
    };

}
