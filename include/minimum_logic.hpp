#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
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

    // ---------------------------------------------------------------------------------------
    // Frame statistics
    // ---------------------------------------------------------------------------------------

    // Keeps the last kCapacity drawn frames: how long each one took from start to start
    // (what the player sees) and how long the CPU was busy inside it. Fed once per drawn
    // frame, read a few times per second by the FPS counter and the Minimum popup.
    class FrameStats {
    public:
        static constexpr size_t kCapacity = 240;

        void push(double frameMs, double cpuMs) {
            m_frame[m_head] = static_cast<float>(frameMs);
            m_cpu[m_head] = static_cast<float>(cpuMs);
            m_head = (m_head + 1) % kCapacity;
            if (m_size < kCapacity) ++m_size;
        }

        void clear() {
            m_head = 0;
            m_size = 0;
        }

        size_t size() const { return m_size; }

        // i = 0 is the oldest sample, size() - 1 the newest.
        double frameMsAt(size_t i) const { return m_frame[slot(i)]; }
        double cpuMsAt(size_t i) const { return m_cpu[slot(i)]; }

        // Average over the newest n samples (n is clamped to what is available).
        double averageFrameMs(size_t n) const { return average(m_frame, n); }
        double averageCpuMs(size_t n) const { return average(m_cpu, n); }

        // "1% low": the frame rate of the slowest 1% of the newest n frames. It is the number
        // that tells you about stutter, which an average FPS hides.
        double lowFps(size_t n) const {
            n = std::min(n, m_size);
            if (n == 0) return 0.0;
            std::array<float, kCapacity> window{};
            for (size_t i = 0; i < n; ++i) {
                window[i] = m_frame[slot(m_size - n + i)];
            }
            size_t const worst = std::max<size_t>(1, (n + 99) / 100);
            // Largest `worst` frame times end up in the last `worst` slots.
            std::nth_element(window.begin(), window.begin() + (n - worst), window.begin() + n);
            double sum = 0.0;
            for (size_t i = n - worst; i < n; ++i) sum += window[i];
            double const avg = sum / static_cast<double>(worst);
            return avg > 0.0 ? 1000.0 / avg : 0.0;
        }

    private:
        size_t slot(size_t i) const { return (m_head + kCapacity - m_size + i) % kCapacity; }

        double average(std::array<float, kCapacity> const& data, size_t n) const {
            n = std::min(n, m_size);
            if (n == 0) return 0.0;
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i) sum += data[slot(m_size - n + i)];
            return sum / static_cast<double>(n);
        }

        std::array<float, kCapacity> m_frame{};
        std::array<float, kCapacity> m_cpu{};
        size_t m_head = 0;
        size_t m_size = 0;
    };

    // Green / yellow / red for the FPS counter, relative to the frame rate the player aims at.
    enum class FpsBand { Good, Okay, Bad };

    inline FpsBand classifyFps(double fps, double targetFps) {
        if (targetFps <= 0.0) targetFps = 60.0;
        double const ratio = fps / targetFps;
        if (ratio >= 0.92) return FpsBand::Good;
        if (ratio >= 0.60) return FpsBand::Okay;
        return FpsBand::Bad;
    }

    // ---------------------------------------------------------------------------------------
    // Presets
    // ---------------------------------------------------------------------------------------

    // The numbers a preset stands for. "Custom" (or anything unknown) is not a preset.
    // Every preset also switches "skip idle particles" and "cap particles" on. Only the
    // strongest one also forces the game's own Low Detail Mode on.
    struct PresetValues {
        bool known = false;
        unsigned int particleCap = 128;
        double backgroundFps = 20.0;
        bool lowDetail = false;
    };

    inline PresetValues presetValues(std::string const& name) {
        if (name == "Balanced") return {true, 256u, 30.0, false};
        if (name == "Performance") return {true, 128u, 20.0, false};
        if (name == "Extreme") return {true, 48u, 10.0, false};
        if (name == "Super Performance") return {true, 16u, 5.0, true};
        return {};
    }

    // ---------------------------------------------------------------------------------------
    // Popup layout
    // ---------------------------------------------------------------------------------------

    // Where everything in the Minimum popup goes. All boxes are in popup coordinates, the
    // origin is the lower left corner of the popup. Built bottom-up, so the popup is exactly as
    // tall as its content. Three spacing levels (0 roomy, 1 compact, 2 tight): the roomiest one
    // that fits the screen is used.
    struct PopupPlan {
        float width = 0.f;
        float height = 0.f;
        int level = 0;
        bool compact = false;      // level >= 1
        Box live;
        std::vector<Box> presets;  // first row on top, 3 per row, the last row is stretched
        std::vector<Box> toggles;
        Box settingsButton;
        Box credits;               // two lines of text
    };

    inline PopupPlan planPopup(float maxHeight, size_t toggleCount, size_t presetCount = 5) {
        auto build = [&](int level) {
            PopupPlan p;
            p.level = level;
            p.compact = level >= 1;
            p.width = 372.f;

            float const side = 18.f;
            float const gap = level == 0 ? 7.f : (level == 1 ? 5.f : 4.f);
            float const titleZone = level == 0 ? 42.f : (level == 1 ? 34.f : 30.f);
            float const liveH = level == 0 ? 54.f : (level == 1 ? 46.f : 40.f);
            float const presetH = level == 0 ? 26.f : (level == 1 ? 24.f : 22.f);
            float const presetGap = level == 0 ? 6.f : 5.f;
            float const cardH = level == 0 ? 28.f : (level == 1 ? 26.f : 24.f);
            float const cardPitch = cardH + (level == 0 ? 4.f : 4.f);
            float const buttonH = level == 0 ? 28.f : (level == 1 ? 26.f : 24.f);
            float const creditsH = 26.f;
            float const bottom = level == 0 ? 10.f : (level == 1 ? 8.f : 6.f);
            float const inner = p.width - 2.f * side;
            float const columnGap = 8.f;
            float const cardW = (inner - columnGap) / 2.f;

            size_t const rows = (toggleCount + 1) / 2;

            float y = bottom;
            p.credits = {side, y, inner, creditsH};
            y += creditsH + gap;

            float const buttonW = 140.f;
            p.settingsButton = {(p.width - buttonW) / 2.f, y, buttonW, buttonH};
            y += buttonH + gap;

            float const blockH = rows == 0 ? 0.f : static_cast<float>(rows - 1) * cardPitch + cardH;
            p.toggles.reserve(toggleCount);
            for (size_t i = 0; i < toggleCount; ++i) {
                size_t const row = i / 2;
                bool const alone = (i + 1 == toggleCount) && (toggleCount % 2 == 1);
                float const x = alone ? (p.width - cardW) / 2.f : (i % 2 == 0 ? side : side + cardW + columnGap);
                float const top = static_cast<float>(rows - 1 - row) * cardPitch;
                p.toggles.push_back({x, y + top, cardW, cardH});
            }
            y += blockH + gap;

            // Presets: 3 per row. Rows are built bottom-up, so the LAST row is placed first,
            // and the boxes are stored in reading order (first row first).
            size_t const perRow = 3;
            size_t const presetRows = presetCount == 0 ? 0 : (presetCount + perRow - 1) / perRow;
            p.presets.assign(presetCount, Box{});
            for (size_t r = presetRows; r-- > 0;) {
                size_t const first = r * perRow;
                size_t const inRow = std::min(perRow, presetCount - first);
                float const w = (inner - presetGap * static_cast<float>(inRow - 1)) / static_cast<float>(inRow);
                for (size_t i = 0; i < inRow; ++i) {
                    p.presets[first + i] = {side + static_cast<float>(i) * (w + presetGap), y, w, presetH};
                }
                y += presetH + (r > 0 ? presetGap : 0.f);
            }
            if (presetRows > 0) y += gap;

            p.live = {side, y, inner, liveH};
            y += liveH + titleZone;

            p.height = y;
            return p;
        };

        for (int level = 0; level < 2; ++level) {
            PopupPlan plan = build(level);
            if (plan.height <= maxHeight) return plan;
        }
        return build(2);
    }

}
