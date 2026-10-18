#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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

    // Decides when the frame queue bound of Low Latency Mode may run.
    //
    // The sync lowers input latency but removes the overlap between CPU and GPU work, so on a
    // GPU bound machine it costs frame rate. The guard switches it off when it sees the frame
    // rate fall, and (this is the important part) it does NOT switch it straight back on:
    // every time it has to back off, it stays off twice as long (10 s, 20 s, 40 s ... max 5 min).
    //
    // A trip is forgiven one at a time: after kForgiveAfterSec of continuous Active without a
    // new trip, the trip count goes down by one (never below zero), so the backoff for a player
    // who had one bad level does not stay at 5 minutes for the rest of the session.
    //
    // The previous version re-enabled the sync one measurement later. The sync made the FPS
    // drop, the guard turned it off, the FPS recovered, the guard turned it on again, and so on,
    // so the frame rate alternated every half second.
    //
    // Call update() once per measurement window (about every 0.5 s).
    class LatencyGate {
    public:
        static constexpr double kForgiveAfterSec = 120.0;
        static constexpr double kMaxLockSec = 300.0;

        // fps    : frames drawn per second in the window that just ended.
        // now    : seconds on any monotonic clock.
        // wanted : the user wants the sync and the game is in a state where it may run.
        bool update(double fps, double now, bool wanted) {
            // Recent best frame rate, decays about 3 % per window so it follows the level.
            m_peak = std::max(fps, m_peak * 0.97);
            double const dt = m_hasLast ? std::max(0.0, now - m_lastNow) : 0.0;
            m_hasLast = true;
            m_lastNow = now;

            if (!wanted || now < m_lockedUntil) {
                m_on = false;
                m_activeFor = 0.0;
                return false;
            }

            if (m_on) {
                m_activeFor += dt;
                if (m_activeFor >= kForgiveAfterSec) {
                    if (m_trips > 0) --m_trips;
                    m_activeFor = 0.0;
                }
                // The sync is running and the frame rate fell clearly below the recent best.
                if (fps < 30.0 || fps < 0.85 * m_peak) {
                    m_on = false;
                    ++m_trips;
                    m_activeFor = 0.0;
                    double const lock = std::min(kMaxLockSec, 10.0 * std::pow(2.0, m_trips - 1));
                    m_lockedUntil = now + lock;
                }
            }
            else if (fps >= 30.0 && fps >= 0.97 * m_peak) {
                // Only start while the game is holding a steady frame rate.
                m_on = true;
                m_activeFor = 0.0;
            }
            return m_on;
        }

        bool active() const { return m_on; }
        int trips() const { return m_trips; }

    private:
        double m_peak = 0.0;
        double m_lockedUntil = 0.0;
        double m_lastNow = 0.0;
        double m_activeFor = 0.0;
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

    // Band of ONE drawn frame against the frame budget (1000 / target FPS): Good within 5% over
    // the budget, Okay up to 50% over it, Bad beyond that. Used to color the frame-time graph.
    inline FpsBand classifyFrameMs(double frameMs, double targetFps) {
        if (targetFps <= 0.0) targetFps = 60.0;
        double const budget = 1000.0 / targetFps;
        if (frameMs <= budget * 1.05) return FpsBand::Good;
        if (frameMs <= budget * 1.5) return FpsBand::Okay;
        return FpsBand::Bad;
    }

    // The frame time that reaches the top of the frame-time graph: twice the frame budget, and
    // never less than 8 ms, so on a 240 FPS target the graph does not become a sliver.
    // A frame on budget therefore sits at half height.
    inline double sparkCeilingMs(double targetFps) {
        if (targetFps <= 0.0) targetFps = 60.0;
        return std::max(8.0, 2000.0 / targetFps);
    }

    // Bar height as a 0..1 fraction of the graph. NaN, negative times and a broken ceiling give 0.
    inline float sparkBarFraction(double frameMs, double ceilingMs) {
        if (!(ceilingMs > 0.0) || !(frameMs > 0.0)) return 0.f;
        return static_cast<float>(std::min(1.0, frameMs / ceilingMs));
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
    // Bottleneck readout
    // ---------------------------------------------------------------------------------------

    // Which side limits the frame: the CPU (the game code and the driver spend the frame busy)
    // or the GPU (the CPU is mostly waiting, which only shows while Low Latency Mode syncs).
    enum class Bottleneck { None, CpuBound, GpuBound };

    constexpr size_t kBottleneckWindow = 60;
    constexpr double kCpuBoundShare = 0.85;
    constexpr double kGpuBoundShare = 0.5;

    // avgCpuMs / avgFrameMs are the means over the newest kBottleneckWindow drawn frames.
    // `samples` is how many drawn frames exist, `syncActive` is Low Latency Mode running right now.
    // Nothing is classified until a full window exists.
    inline Bottleneck classifyBottleneck(double avgCpuMs, double avgFrameMs, size_t samples, bool syncActive) {
        if (samples < kBottleneckWindow || avgFrameMs <= 0.0) return Bottleneck::None;
        double const share = avgCpuMs / avgFrameMs;
        if (share >= kCpuBoundShare) return Bottleneck::CpuBound;
        if (share < kGpuBoundShare && syncActive) return Bottleneck::GpuBound;
        return Bottleneck::None;
    }

    inline char const* bottleneckLabel(Bottleneck b) {
        switch (b) {
            case Bottleneck::CpuBound: return "CPU bound";
            case Bottleneck::GpuBound: return "GPU bound";
            default: return "";
        }
    }

    // ---------------------------------------------------------------------------------------
    // Particle cap steps
    // ---------------------------------------------------------------------------------------

    // Steps the particle cap down (cap, cap/2, cap/4, cap/8) while the game keeps missing the
    // target frame rate, and back up once it is smooth again. Fed once per second with the
    // number of drawn frames in that second and how many of them were slow.
    class AdaptiveCapGovernor {
    public:
        static constexpr int kMaxStep = 3;

        void noteWindow(uint32_t frames, uint32_t slowFrames) {
            if (frames < 10) return;
            double const slowRatio = static_cast<double>(slowFrames) / static_cast<double>(frames);
            if (slowRatio > 0.25) {
                m_goodWindows = 0;
                // Two bad seconds in a row before lowering, so one hitch does not change anything.
                if (++m_badWindows >= 2) {
                    m_badWindows = 0;
                    if (m_step < kMaxStep) ++m_step;
                }
            }
            else if (slowRatio < 0.02) {
                m_badWindows = 0;
                // Ten smooth seconds before raising the cap again.
                if (++m_goodWindows >= 10) {
                    m_goodWindows = 0;
                    if (m_step > 0) --m_step;
                }
            }
            else {
                m_badWindows = 0;
            }
        }

        void reset() { *this = AdaptiveCapGovernor{}; }
        int step() const { return m_step; }

    private:
        int m_step = 0;
        int m_badWindows = 0;
        int m_goodWindows = 0;
    };

    // A drawn frame counts as slow when it took longer than 1.5 times the target frame time.
    // `frameSec` is the time between two frames of the game loop (not only the drawn ones).
    inline bool isSlowFrame(double frameSec, double targetFps) {
        if (targetFps <= 0.0) targetFps = 60.0;
        return frameSec * 1000.0 > 1500.0 / targetFps;
    }

    // Per second rate of a counter that grew by `delta` over `spanSec` seconds. 0 for an empty span.
    inline uint64_t perSecond(uint64_t delta, double spanSec) {
        if (spanSec <= 0.0) return 0;
        return static_cast<uint64_t>(static_cast<double>(delta) / spanSec + 0.5);
    }

    // Thermal steps. The values are AThermalStatus from the NDK (NONE 0, LIGHT 1, MODERATE 2,
    // SEVERE 3, CRITICAL 4, EMERGENCY 5, SHUTDOWN 6; ERROR -1).
    //
    // While the status is SEVERE or worse the cap drops one step at once, then one more step at
    // most every kStepEverySec. After kRecoverAfterSec of nominal status (NONE or LIGHT) it comes
    // back one step, and again after each further kRecoverAfterSec. MODERATE and ERROR hold the
    // current step and restart the nominal timer.
    class ThermalGovernor {
    public:
        static constexpr int kMaxStep = 3;
        static constexpr int kSevere = 3;
        static constexpr double kStepEverySec = 5.0;
        static constexpr double kRecoverAfterSec = 30.0;

        int update(int status, double now) {
            if (status >= kSevere) {
                m_nominalSince = -1.0;
                bool const due = m_lastStepAt < 0.0 || now - m_lastStepAt >= kStepEverySec;
                if (due && m_step < kMaxStep) {
                    ++m_step;
                    m_lastStepAt = now;
                }
            }
            else if (status == 0 || status == 1) {
                if (m_nominalSince < 0.0) m_nominalSince = now;
                if (m_step > 0 && now - m_nominalSince >= kRecoverAfterSec) {
                    --m_step;
                    m_nominalSince = now;
                }
            }
            else {
                m_nominalSince = -1.0;
            }
            return m_step;
        }

        void reset() { *this = ThermalGovernor{}; }
        int step() const { return m_step; }

    private:
        int m_step = 0;
        double m_lastStepAt = -1.0;
        double m_nominalSince = -1.0;
    };

    // The adaptive and the thermal governor lower the same cap, so their steps add up and the
    // total is capped at the deepest step.
    inline int combinedCapStep(int adaptiveStep, int thermalStep) {
        return std::clamp(std::max(0, adaptiveStep) + std::max(0, thermalStep), 0, AdaptiveCapGovernor::kMaxStep);
    }

    // The configured pool size lowered by `step` halvings. Never below 16, never above `configured`.
    inline unsigned int cappedPoolSize(unsigned int configured, int step) {
        if (step <= 0) return configured;
        unsigned int const lowered = std::max(configured >> std::min(step, 8), 16u);
        return std::min(configured, lowered);
    }

    // ---------------------------------------------------------------------------------------
    // Spawn budget
    // ---------------------------------------------------------------------------------------

    // Particle systems past the per-frame budget still get created (the game expects them to
    // exist), they just get this small pool instead of a full one.
    constexpr unsigned int kOverBudgetPool = 4;

    // `createdEarlier` is how many particle systems were already created in this frame.
    inline unsigned int budgetedPoolSize(unsigned int requested, uint64_t createdEarlier, unsigned int budget) {
        if (budget == 0 || createdEarlier < budget) return requested;
        return std::min(requested, kOverBudgetPool);
    }

    // ---------------------------------------------------------------------------------------
    // Off-screen particle culling
    // ---------------------------------------------------------------------------------------

    // Extra room around a system, on top of its particle size, so nothing is cut at the edge.
    constexpr float kCullMarginPt = 48.f;

    // Upper bounds for one particle system, all in its own (local) units. Every value is a bound,
    // not an average: speed and life include their variance, acceleration is the sum of the
    // magnitudes of gravity, radial and tangential acceleration (variances included), radius is the
    // largest orbit of radius mode, and the sizes are the larger of start and end size.
    struct ParticleBounds {
        float speed = 0.f;         // speed + |speed variance|
        float life = 0.f;          // life + |life variance|
        float accel = 0.f;         // |gravity| + radial + tangential, each with its variance
        float radius = 0.f;        // largest orbit radius (radius mode), variance included
        float posVar = 0.f;        // length of the position variance
        float sourceLength = 0.f;  // length of the emitter offset
        float maxSize = 0.f;       // larger of start and end size, variance included
    };

    // How far a particle can get from its emitter, in local units. For constant acceleration
    // |x(t)| <= v * t + a * t * t / 2, and both speed and acceleration are taken at their bounds,
    // so this holds for every particle. Radius mode moves particles on a circle of radius at most
    // `radius`. Both are computed and the larger one is used, so a system that uses both modes is
    // still covered. The margin covers the discrete integration steps and texture edges.
    inline float particleReach(ParticleBounds const& b) {
        float const life = std::max(0.f, b.life);
        float const travel = std::max(0.f, b.speed) * life + 0.5f * std::max(0.f, b.accel) * life * life;
        float const orbit = std::max(0.f, b.radius);
        return std::max(0.f, b.sourceLength) + std::max(0.f, b.posVar)
             + std::max(travel, orbit) + std::max(0.f, b.maxSize) + kCullMarginPt;
    }

    // True when the square of half-size `reach` around `center` touches `area` (edges included,
    // so a particle exactly on the border is never culled).
    inline bool squareTouches(Vec2 center, float reach, Box const& area) {
        return center.x + reach >= area.x && center.x - reach <= area.right()
            && center.y + reach >= area.y && center.y - reach <= area.top();
    }

    // ---------------------------------------------------------------------------------------
    // Frame rate limits
    // ---------------------------------------------------------------------------------------

    // The animation interval (seconds per frame) the game should ask the OS for. With the unlock
    // on, a request slower than the limit is shortened to the limit. A faster request is kept.
    inline double unlockedAnimationInterval(double requested, bool unlock, int limitFps) {
        if (!unlock || requested <= 0.0) return requested;
        int const fps = std::clamp(limitFps, 30, 240);
        double const fastest = 1.0 / static_cast<double>(fps);
        return requested > fastest ? fastest : requested;
    }

    // Seconds still to wait before the next frame may start, 0 when it is already late.
    inline double frameWaitSeconds(double elapsedSec, double periodSec) {
        double const left = periodSec - elapsedSec;
        return left > 0.0 ? left : 0.0;
    }

    struct WaitSplit {
        double sleepSec = 0.0;
        double spinSec = 0.0;
    };

    // Splits a wait into a sleep and a busy wait. The last `spinSec` are spun, because a sleep
    // is only as exact as the OS timer.
    inline WaitSplit splitWait(double waitSec, double spinSec) {
        if (waitSec <= 0.0) return {};
        if (waitSec <= spinSec) return {0.0, waitSec};
        return {waitSec - spinSec, spinSec};
    }

    // ---------------------------------------------------------------------------------------
    // Hidden shortcut and benchmark recording
    // ---------------------------------------------------------------------------------------

    // Counts taps that come quickly after each other. `tap` returns true on the tap that
    // completes a streak of `needed` taps, each one within `windowSec` of the streak's first tap.
    class TapStreak {
    public:
        TapStreak(int needed, double windowSec) : m_needed(needed), m_window(windowSec) {}

        bool tap(double now) {
            if (m_count == 0 || now - m_first > m_window) {
                m_first = now;
                m_count = 0;
            }
            ++m_count;
            if (m_count >= m_needed) {
                m_count = 0;
                return true;
            }
            return false;
        }

    private:
        int m_needed;
        double m_window;
        double m_first = 0.0;
        int m_count = 0;
    };

    // Records frame times for a fixed time and writes them out as CSV text.
    class FrameRecorder {
    public:
        struct Sample {
            double elapsedSec;
            double frameMs;
            double cpuMs;
        };

        void start(double now, double durationSec) {
            m_samples.clear();
            m_samples.reserve(static_cast<size_t>(durationSec * 240.0));
            m_start = now;
            m_duration = durationSec;
            m_active = true;
        }

        // Returns true on the sample that ends the recording.
        bool add(double now, double frameMs, double cpuMs) {
            if (!m_active) return false;
            m_samples.push_back({now - m_start, frameMs, cpuMs});
            if (now - m_start >= m_duration) {
                m_active = false;
                return true;
            }
            return false;
        }

        void stop() { m_active = false; }
        bool active() const { return m_active; }
        size_t size() const { return m_samples.size(); }

        double remainingSec(double now) const {
            return m_active ? std::max(0.0, m_duration - (now - m_start)) : 0.0;
        }

        std::string csv() const {
            std::string out = "elapsed_s,frame_ms,cpu_ms\n";
            char line[96];
            for (auto const& s : m_samples) {
                std::snprintf(line, sizeof(line), "%.4f,%.3f,%.3f\n", s.elapsedSec, s.frameMs, s.cpuMs);
                out += line;
            }
            return out;
        }

    private:
        std::vector<Sample> m_samples;
        double m_start = 0.0;
        double m_duration = 0.0;
        bool m_active = false;
    };

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
    // that fits the screen is used. If even the tightest one is taller than the screen, the
    // whole popup is scaled by `fitScale` so it still fits.
    struct PopupPlan {
        float width = 0.f;
        float height = 0.f;        // unscaled
        float fitScale = 1.f;      // min(1, maxHeight / height)
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

        PopupPlan plan = build(2);
        for (int level = 0; level < 2; ++level) {
            PopupPlan candidate = build(level);
            if (candidate.height <= maxHeight) {
                plan = candidate;
                break;
            }
        }
        if (plan.height > 0.f) {
            plan.fitScale = std::clamp(maxHeight / plan.height, 0.05f, 1.f);
        }
        return plan;
    }

}
