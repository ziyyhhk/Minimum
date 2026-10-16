#include <Geode/Geode.hpp>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <minimum.hpp>

using namespace geode::prelude;

// Benchmark mode.
//
// A hidden switch (tap the logo in the Minimum popup five times) records the frame time of every
// drawn frame for 60 seconds and writes them to a CSV in the mod's save directory:
//
//   frame,time_s,frame_ms
//
// frame_ms is the time from the start of the previous drawn frame to the start of this one, the
// same number the FPS counter and the 1% low are built from. Frames that are not measured (the
// first 300 while loading, while the window is unfocused, right after the game was away) are not
// recorded, so a run only contains frames the stats line would also count.
//
// Nothing here runs per frame except one atomic load while no benchmark is active.

namespace minimum {

    namespace {

        BenchmarkRecorder g_recorder;
        std::atomic<bool> g_active{false};

        double nowSeconds() {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        void finish() {
            g_active.store(false, std::memory_order_relaxed);

            if (g_recorder.samples() == 0) {
                Notification::create("Benchmark: no frames recorded", NotificationIcon::Error, 2.f)->show();
                return;
            }

            std::error_code ec;
            auto const dir = Mod::get()->getSaveDir();
            std::filesystem::create_directories(dir, ec);

            auto const file = dir / fmt::format("benchmark-{}.csv", static_cast<long long>(std::time(nullptr)));
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            bool ok = false;
            if (out) {
                std::string const csv = g_recorder.toCsv();
                out.write(csv.data(), static_cast<std::streamsize>(csv.size()));
                out.flush();
                ok = static_cast<bool>(out);
            }

            double const avgMs = g_recorder.averageFrameMs();
            std::string const where = geode::utils::string::pathToString(file);
            if (ok) {
                log::info(
                    "Minimum benchmark: {} frames, average {:.2f} ms ({:.1f} FPS), saved to {}",
                    g_recorder.samples(), avgMs, avgMs > 0.0 ? 1000.0 / avgMs : 0.0, where
                );
                Notification::create("Benchmark saved (see the Minimum save folder)", NotificationIcon::Success, 3.f)->show();
            }
            else {
                log::error("Minimum benchmark: could not write {}", where);
                Notification::create("Benchmark: could not write the CSV", NotificationIcon::Error, 3.f)->show();
            }
        }

    }

    bool benchmarkActive() {
        return g_active.load(std::memory_order_relaxed);
    }

    void benchmarkToggle() {
        if (g_active.load(std::memory_order_relaxed)) {
            g_recorder.cancel();
            g_active.store(false, std::memory_order_relaxed);
            Notification::create("Benchmark cancelled", NotificationIcon::Info, 1.5f)->show();
            return;
        }
        g_recorder.start(nowSeconds());
        g_active.store(true, std::memory_order_relaxed);
        Notification::create("Benchmark: recording 60 s", NotificationIcon::Info, 2.f)->show();
    }

    void benchmarkFrame(double nowSeconds, double frameMs) {
        if (!g_active.load(std::memory_order_relaxed)) return;
        if (g_recorder.push(nowSeconds, frameMs)) finish();
    }

    double benchmarkElapsed(double nowSeconds) {
        return g_active.load(std::memory_order_relaxed) ? g_recorder.elapsed(nowSeconds) : 0.0;
    }

}
