#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
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
    // Trip decay: m_trips used to only grow, so one bad minute at the start of a session kept the
    // backoff at its maximum for the rest of it. Now every kDecaySeconds of Active time without a
    // trip forgives one trip (never below 0), so the lock after a later trip is shorter again.
    // Only time spent Active counts, and a trip restarts the clock.
    //
    // Call update() once per measurement window (about every 0.5 s).
    class LatencyGate {
    public:
        static constexpr double kDecaySeconds = 120.0;
        // One window is ~0.5 s. A longer gap (a hitch, the game was away) is not credited in full.
        static constexpr double kMaxCreditSeconds = 1.0;

        // fps    : frames drawn per second in the window that just ended.
        // now    : seconds on any monotonic clock.
        // wanted : the user wants the sync and the game is in a state where it may run.
        bool update(double fps, double now, bool wanted) {
            // The window that just ended counts as clean Active time when the sync was on
            // during it. It is credited before this window's verdict, so a trip below wipes it.
            if (m_hasLast && m_on && now > m_lastNow) {
                creditActive(std::min(now - m_lastNow, kMaxCreditSeconds));
            }
            m_lastNow = now;
            m_hasLast = true;

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
                    m_cleanSeconds = 0.0;
                    double const lock = std::min(300.0, 10.0 * std::pow(2.0, std::min(m_trips, 16) - 1));
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
        void creditActive(double seconds) {
            if (m_trips <= 0) {
                m_cleanSeconds = 0.0;
                return;
            }
            m_cleanSeconds += seconds;
            while (m_trips > 0 && m_cleanSeconds >= kDecaySeconds) {
                --m_trips;
                m_cleanSeconds -= kDecaySeconds;
            }
            if (m_trips == 0) m_cleanSeconds = 0.0;
        }

        double m_peak = 0.0;
        double m_lockedUntil = 0.0;
        double m_lastNow = 0.0;
        double m_cleanSeconds = 0.0;
        int m_trips = 0;
        bool m_on = false;
        bool m_hasLast = false;
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

        // Share of the frame time the CPU was busy, over the newest n samples: sum(cpu) divided by
        // sum(frame), clamped to 0..1. Returns -1 when there are no samples (or no frame time).
        double cpuShare(size_t n) const {
            n = std::min(n, m_size);
            if (n == 0) return -1.0;
            double frame = 0.0;
            double cpu = 0.0;
            for (size_t i = 0; i < n; ++i) {
                size_t const at = slot(m_size - n + i);
                frame += m_frame[at];
                cpu += m_cpu[at];
            }
            if (frame <= 0.0) return -1.0;
            return std::clamp(cpu / frame, 0.0, 1.0);
        }

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

    // The stats line must stay on screen. `requested` is the HUD Scale setting, content* is the
    // label's size at scale 1 and room* is the space available (screen minus margins). The scale is
    // lowered just enough to fit both ways, never raised, and never below 0.1 (or `requested`, if that
    // is smaller). The Detailed line is three lines of ~100 characters, which at a high HUD Scale
    // is wider than a phone screen.
    inline float hudFitScale(float requested, float contentW, float contentH, float roomW, float roomH) {
        float scale = requested;
        if (contentW > 0.f && roomW > 0.f && contentW * scale > roomW) scale = roomW / contentW;
        if (contentH > 0.f && roomH > 0.f && contentH * scale > roomH) scale = std::min(scale, roomH / contentH);
        return std::clamp(scale, std::min(requested, 0.1f), requested);
    }

    // The game can run its logic (scheduler updates) at a different rate than it draws frames,
    // for example 240 updates per second with 60 frames drawn. Other FPS counters often show
    // the update rate, so when the two are clearly apart the counter shows both.
    inline bool logicRateDiffers(double drawFps, double logicRate) {
        if (drawFps <= 0.0 || logicRate <= 0.0) return false;
        double const tolerance = std::max(4.0, drawFps * 0.10);
        return std::fabs(logicRate - drawFps) > tolerance;
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
        // min(1, maxHeight / height). 1 when the plan fits, less when even the tightest spacing
        // level is too tall: the popup node is scaled by this so it always fits the screen.
        float fitScale = 1.f;
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
            float const cardPitch = cardH + 4.f;
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
        // Even the tightest level does not fit: keep it and shrink the whole popup instead of
        // letting it run off the screen. The lower bound only guards against a nonsense maxHeight.
        PopupPlan plan = build(2);
        if (plan.height > 0.f) plan.fitScale = std::clamp(maxHeight / plan.height, 0.1f, 1.f);
        return plan;
    }

    // ---------------------------------------------------------------------------------------
    // Adaptive particle cap
    // ---------------------------------------------------------------------------------------

    constexpr int kMaxCapStep = 3;
    constexpr unsigned int kMinParticleCap = 16;

    // The cap for a step: step 0 is the configured cap, every further step halves it, never
    // below kMinParticleCap and never above the configured cap.
    inline unsigned int capForStep(unsigned int base, int step) {
        if (step <= 0) return base;
        unsigned int const lowered = std::max(base >> std::min(step, 31), kMinParticleCap);
        return std::min(base, lowered);
    }

    // Steps that come from different sources (the adaptive cap and the thermal guard) add up,
    // the total never goes past kMaxCapStep.
    inline int combinedCapStep(int adaptiveStep, int thermalSteps) {
        return std::clamp(adaptiveStep + thermalSteps, 0, kMaxCapStep);
    }

    // A drawn frame is "slow" when it took more than 1.5x the time it should have. That is the
    // target frame time, or, when the frame gate deliberately draws less often than the target
    // (Draw Divide), the gate's own interval. Without the second part every frame of a 30 FPS
    // Draw Divide would count as slow against a 60 FPS target.
    inline double slowFrameThresholdMs(double targetFps, double gateIntervalSeconds) {
        if (!(targetFps > 0.0)) targetFps = 60.0;
        double const expectedMs = std::max(1000.0 / targetFps, gateIntervalSeconds * 1000.0);
        return 1.5 * expectedMs;
    }

    inline bool isSlowFrame(double frameMs, double targetFps, double gateIntervalSeconds) {
        return frameMs > slowFrameThresholdMs(targetFps, gateIntervalSeconds);
    }

    // Lowers the cap when the game keeps missing the target and raises it again after it has
    // been smooth for a while. Feed update() once per second with the number of DRAWN frames in
    // that second and how many of them were slow. Logic-only frames must not be counted: they
    // are cheap by design and would dilute the slow ratio.
    class AdaptiveCap {
    public:
        static constexpr uint32_t kMinFrames = 10;
        static constexpr double kSlowRatio = 0.25;     // above this the second is "bad"
        static constexpr double kSmoothRatio = 0.02;   // below this the second is "smooth"
        static constexpr int kBadSecondsToLower = 2;   // one hitch changes nothing
        static constexpr int kSmoothSecondsToRaise = 10;

        void reset() {
            m_step = 0;
            m_bad = 0;
            m_good = 0;
        }

        void update(uint32_t drawnFrames, uint32_t slowFrames) {
            if (drawnFrames < kMinFrames) return;
            double const ratio = static_cast<double>(slowFrames) / static_cast<double>(drawnFrames);
            if (ratio > kSlowRatio) {
                m_good = 0;
                if (++m_bad >= kBadSecondsToLower) {
                    m_bad = 0;
                    if (m_step < kMaxCapStep) ++m_step;
                }
            }
            else if (ratio < kSmoothRatio) {
                m_bad = 0;
                if (++m_good >= kSmoothSecondsToRaise) {
                    m_good = 0;
                    if (m_step > 0) --m_step;
                }
            }
            else {
                m_bad = 0;
            }
        }

        int step() const { return m_step; }

    private:
        int m_step = 0;
        int m_bad = 0;
        int m_good = 0;
    };

    // ---------------------------------------------------------------------------------------
    // Thermal response
    // ---------------------------------------------------------------------------------------

    // Values of the Android thermal status (AThermalStatus). Kept as plain numbers here so this
    // header needs no NDK: 0 none, 1 light, 2 moderate, 3 severe, 4 critical, 5 emergency,
    // 6 shutdown, -1 error / unknown.
    constexpr int kThermalSevere = 3;
    constexpr int kThermalNominalMax = 1;   // "nominal" = none or light

    // Lowers the particle cap by one step while the device is severely hot, and gives it back
    // after it has been nominal for kRecoverSeconds in a row. Moderate heat changes nothing and
    // does not start the recovery clock; an unknown status (-1) is ignored.
    class ThermalGuard {
    public:
        static constexpr double kRecoverSeconds = 30.0;

        void update(int status, double now) {
            if (status < 0) return;
            if (status >= kThermalSevere) {
                m_throttling = true;
                m_nominal = false;
                return;
            }
            if (!m_throttling) return;
            if (status <= kThermalNominalMax) {
                if (!m_nominal) {
                    m_nominal = true;
                    m_nominalSince = now;
                }
                else if (now - m_nominalSince >= kRecoverSeconds) {
                    m_throttling = false;
                    m_nominal = false;
                }
            }
            else {
                m_nominal = false;
            }
        }

        void reset() {
            m_throttling = false;
            m_nominal = false;
        }

        bool throttling() const { return m_throttling; }
        // How many cap steps to take off (see combinedCapStep).
        int steps() const { return m_throttling ? 1 : 0; }

    private:
        bool m_throttling = false;
        bool m_nominal = false;
        double m_nominalSince = 0.0;
    };

    // ---------------------------------------------------------------------------------------
    // Bottleneck readout
    // ---------------------------------------------------------------------------------------

    enum class Bottleneck : uint8_t { None = 0, Cpu = 1, Gpu = 2 };

    constexpr size_t kBottleneckWindow = 60;       // frames
    constexpr size_t kBottleneckMinFrames = 30;    // fewer than this is not enough to say anything
    constexpr double kCpuBoundShare = 0.85;
    constexpr double kGpuBoundShare = 0.5;

    // cpuShare is FrameStats::cpuShare (< 0 means "no data"). GPU-bound is only claimed while the
    // Low Latency sync is Active: only then does the sync make the GPU wait show up as time the
    // CPU is NOT busy. Without it a low share could just as well be vsync or a frame cap.
    inline Bottleneck classifyBottleneck(double cpuShare, bool latencySyncActive) {
        if (cpuShare < 0.0) return Bottleneck::None;
        if (cpuShare >= kCpuBoundShare) return Bottleneck::Cpu;
        if (cpuShare < kGpuBoundShare && latencySyncActive) return Bottleneck::Gpu;
        return Bottleneck::None;
    }

    inline Bottleneck bottleneckFor(FrameStats const& stats, bool latencySyncActive) {
        if (stats.size() < kBottleneckMinFrames) return Bottleneck::None;
        return classifyBottleneck(stats.cpuShare(kBottleneckWindow), latencySyncActive);
    }

    inline std::string_view bottleneckLabel(Bottleneck b) {
        switch (b) {
            case Bottleneck::Cpu: return "CPU-bound";
            case Bottleneck::Gpu: return "GPU-bound";
            default: return "";
        }
    }

    // ---------------------------------------------------------------------------------------
    // Off-screen particle culling
    // ---------------------------------------------------------------------------------------

    // 2D affine transform, the same layout the game's node transforms use:
    // x' = a*x + c*y + tx, y' = b*x + d*y + ty.
    struct Affine {
        float a = 1.f;
        float b = 0.f;
        float c = 0.f;
        float d = 1.f;
        float tx = 0.f;
        float ty = 0.f;
    };

    inline Vec2 applyAffine(Affine const& t, Vec2 p) {
        return {t.a * p.x + t.c * p.y + t.tx, t.b * p.x + t.d * p.y + t.ty};
    }

    // The transform that moves (0,0) to `origin`, (1,0) to `unitX` and (0,1) to `unitY`. Lets
    // the game side read a node's local-to-world transform with three point conversions.
    inline Affine affineFromUnitPoints(Vec2 origin, Vec2 unitX, Vec2 unitY) {
        return {unitX.x - origin.x, unitX.y - origin.y, unitY.x - origin.x, unitY.y - origin.y, origin.x, origin.y};
    }

    // Axis aligned box around the four transformed corners of `local`.
    inline Box transformBox(Box const& local, Affine const& t) {
        Vec2 const corners[4] = {
            applyAffine(t, {local.x, local.y}),
            applyAffine(t, {local.right(), local.y}),
            applyAffine(t, {local.x, local.top()}),
            applyAffine(t, {local.right(), local.top()}),
        };
        float minX = corners[0].x, maxX = corners[0].x, minY = corners[0].y, maxY = corners[0].y;
        for (auto const& c : corners) {
            minX = std::min(minX, c.x);
            maxX = std::max(maxX, c.x);
            minY = std::min(minY, c.y);
            maxY = std::max(maxY, c.y);
        }
        return {minX, minY, maxX - minX, maxY - minY};
    }

    // Collects the bounds of a set of points. Anything that is not a finite number is ignored,
    // so a corrupt vertex can never produce a box that wrongly claims "off screen".
    class BoundsAccumulator {
    public:
        void add(float x, float y) {
            if (!std::isfinite(x) || !std::isfinite(y)) {
                m_bad = true;
                return;
            }
            if (!m_any) {
                m_minX = m_maxX = x;
                m_minY = m_maxY = y;
                m_any = true;
                return;
            }
            m_minX = std::min(m_minX, x);
            m_maxX = std::max(m_maxX, x);
            m_minY = std::min(m_minY, y);
            m_maxY = std::max(m_maxY, y);
        }

        // False when nothing was added, or when any added value was not a finite number.
        bool valid() const { return m_any && !m_bad; }

        Box box() const { return {m_minX, m_minY, m_maxX - m_minX, m_maxY - m_minY}; }

    private:
        float m_minX = 0.f, m_maxX = 0.f, m_minY = 0.f, m_maxY = 0.f;
        bool m_any = false;
        bool m_bad = false;
    };

    // Safety margin on top of the measured bounds (world units): covers a camera that moves
    // between the particle update and the draw, and rounding in the transform.
    constexpr float kCullExtraMargin = 32.f;

    // boxesCollide() is strict, so a box that sits exactly at the margin would count as "does not
    // overlap". Culling is only allowed for boxes clearly beyond the margin; a borderline case is drawn.
    constexpr float kCullEpsilon = 0.01f;

    // Margin that makes sure the biggest particle the system can have is never clipped: its
    // largest size plus kCullExtraMargin.
    inline float particleCullMargin(float startSize, float startSizeVar, float endSize, float endSizeVar) {
        auto const sane = [](float v) { return std::isfinite(v) ? std::fabs(v) : 0.f; };
        float const biggest = std::max(sane(startSize) + sane(startSizeVar), sane(endSize) + sane(endSizeVar));
        return biggest + kCullExtraMargin;
    }

    // True when the system's draw can be skipped: culling is on, the bounds are trustworthy and
    // their world-space box stays clear of `visible` by more than `margin`.
    inline bool shouldCullParticles(
        bool enabled, BoundsAccumulator const& localBounds, Affine const& localToWorld,
        Box const& visible, float margin
    ) {
        if (!enabled || !localBounds.valid()) return false;
        Box const world = transformBox(localBounds.box(), localToWorld);
        if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(world.w) || !std::isfinite(world.h)) {
            return false;
        }
        return !boxesCollide(world, visible, margin + kCullEpsilon);
    }

    // ---------------------------------------------------------------------------------------
    // Spawn budget
    // ---------------------------------------------------------------------------------------

    // Pool size of a particle system that is created after the per-frame budget is used up. It
    // is still created (never a failed create, the game does not expect one), it just has next
    // to no particles.
    constexpr unsigned int kOverBudgetPoolSize = 4;

    // The budget is for gameplay, not for loading. A level creates hundreds of systems in one
    // frame while it loads, and giving those a tiny pool would ruin the level's own effects for
    // good. So the budget only starts once the player has been in the level for this many
    // frames in a row (counted at the start of each frame, so the loading frame itself never
    // counts).
    constexpr uint32_t kSpawnBudgetGraceFrames = 60;

    inline bool spawnBudgetActive(uint32_t framesInLevel) {
        return framesInLevel >= kSpawnBudgetGraceFrames;
    }

    // budget <= 0 means off. `spentThisFrame` is how many systems were already created in this
    // frame before the one being created now.
    inline unsigned int spawnBudgetTotal(unsigned int requested, uint64_t spentThisFrame, int budget) {
        if (budget <= 0) return requested;
        if (spentThisFrame < static_cast<uint64_t>(budget)) return requested;
        return std::min(requested, kOverBudgetPoolSize);
    }

    // ---------------------------------------------------------------------------------------
    // Frame rate unlock (Android / iOS)
    // ---------------------------------------------------------------------------------------

    constexpr int kFpsLimitMin = 30;
    constexpr int kFpsLimitMax = 240;

    // The animation interval (seconds) to hand to the system. The game's own request goes
    // through unchanged when the unlock is off, and when it already asks for at least as many
    // frames as the limit. Requests of 0 or less mean "no interval" and are never touched.
    inline double unlockedInterval(double requested, bool enabled, int limit) {
        if (!enabled || !(requested > 0.0)) return requested;
        int const fps = std::clamp(limit, kFpsLimitMin, kFpsLimitMax);
        return std::min(requested, 1.0 / static_cast<double>(fps));
    }

    // ---------------------------------------------------------------------------------------
    // Precise frame limiter (Windows / macOS)
    // ---------------------------------------------------------------------------------------

    // The limiter only runs when the game's own cap is not already holding the frame rate at the
    // target: the game's animation interval is clearly shorter than the target's (or 0 = no cap).
    // Two limiters on top of each other make frame times jitter.
    inline bool shouldPreciseLimit(bool enabled, double targetFps, double gameIntervalSeconds) {
        if (!enabled || !(targetFps > 0.0) || std::isnan(gameIntervalSeconds)) return false;
        return gameIntervalSeconds < 0.9 / targetFps;
    }

    // The deadline for the end of this frame. Normally one interval after the previous
    // deadline, which keeps the pace exact over time. When the frame ran late the deadline is
    // "now": the frame that was lost is not made up with a burst of fast ones.
    inline double nextDeadline(double previousDeadline, double now, double interval) {
        return std::max(previousDeadline + interval, now);
    }

    struct WaitPlan {
        double sleepSeconds = 0.0;   // hand the thread to the OS for this long ...
        double spinSeconds = 0.0;    // ... then busy-wait this long, the OS wake-up is not exact
    };

    inline WaitPlan planWait(double now, double deadline, double spinWindow) {
        double const remaining = deadline - now;
        if (!(remaining > 0.0)) return {};
        double const spin = std::min(std::max(spinWindow, 0.0), remaining);
        return {remaining - spin, spin};
    }

    // ---------------------------------------------------------------------------------------
    // Counters per second
    // ---------------------------------------------------------------------------------------

    inline uint32_t ratePerSecond(uint64_t delta, double spanSeconds) {
        if (!(spanSeconds > 0.0)) return 0;
        double const rate = static_cast<double>(delta) / spanSeconds;
        return rate >= 4.0e9 ? 4000000000u : static_cast<uint32_t>(rate + 0.5);
    }

    // ---------------------------------------------------------------------------------------
    // Benchmark mode
    // ---------------------------------------------------------------------------------------

    // Records the frame time of every drawn frame for kDurationSeconds, then reports "finished".
    class BenchmarkRecorder {
    public:
        static constexpr double kDurationSeconds = 60.0;

        void start(double now) {
            m_active = true;
            m_start = now;
            m_time.clear();
            m_frameMs.clear();
        }

        void cancel() { m_active = false; }

        bool active() const { return m_active; }
        double elapsed(double now) const { return m_active ? std::max(0.0, now - m_start) : 0.0; }
        size_t samples() const { return m_frameMs.size(); }

        // Returns true exactly once: on the call that ends the run.
        bool push(double now, double frameMs) {
            if (!m_active) return false;
            if (now - m_start >= kDurationSeconds) {
                m_active = false;
                return true;
            }
            m_time.push_back(now - m_start);
            m_frameMs.push_back(frameMs);
            return false;
        }

        double averageFrameMs() const {
            if (m_frameMs.empty()) return 0.0;
            double sum = 0.0;
            for (double v : m_frameMs) sum += v;
            return sum / static_cast<double>(m_frameMs.size());
        }

        std::string toCsv() const {
            std::string out = "frame,time_s,frame_ms\n";
            out.reserve(out.size() + m_frameMs.size() * 24);
            char line[64];
            for (size_t i = 0; i < m_frameMs.size(); ++i) {
                int const n = std::snprintf(line, sizeof(line), "%zu,%.4f,%.4f\n", i, m_time[i], m_frameMs[i]);
                if (n > 0) out.append(line, static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(line)) - 1)));
            }
            return out;
        }

    private:
        std::vector<double> m_time;
        std::vector<double> m_frameMs;
        double m_start = 0.0;
        bool m_active = false;
    };

    // N taps within a time window. Used for the hidden benchmark switch (tap the popup logo).
    class TapCounter {
    public:
        explicit TapCounter(size_t taps = 5, double windowSeconds = 3.0)
            : m_taps(taps), m_window(windowSeconds) {}

        // Returns true on the tap that completes the sequence, and starts over.
        bool tap(double now) {
            m_times.erase(
                std::remove_if(m_times.begin(), m_times.end(), [&](double t) { return now - t > m_window; }),
                m_times.end()
            );
            m_times.push_back(now);
            if (m_times.size() >= m_taps) {
                m_times.clear();
                return true;
            }
            return false;
        }

    private:
        std::vector<double> m_times;
        size_t m_taps;
        double m_window;
    };

}
