#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <minimum.hpp>
#include <minimum_logic.hpp>

using namespace geode::prelude;

// Benchmark mode (hidden, all platforms).
//
// Five quick taps on the live FPS card of the Minimum popup start a 60 second recording. Every
// measured drawn frame adds one row: seconds since the start, frame time and CPU time. Five more
// taps while it runs stop it early and keep what was recorded. The result is a CSV in the Geode
// save directory of the mod, named benchmark-<unix time>.csv. Nothing is recorded unless the
// taps happen, and nothing is written during the recording itself (the file is written once, at
// the end).

namespace minimum {

    namespace {

        constexpr double kBenchmarkSec = 60.0;

        TapStreak g_taps(5, 2.0);
        FrameRecorder g_recorder;

        void notify(std::string const& text, NotificationIcon icon) {
            Notification::create(text, icon, 2.f)->show();
        }

        void saveRecording() {
            std::size_t const rows = g_recorder.size();
            if (rows == 0) {
                notify("Benchmark stopped, nothing recorded yet", NotificationIcon::Info);
                return;
            }

            std::error_code ec;
            auto const dir = Mod::get()->getSaveDir();
            std::filesystem::create_directories(dir, ec);

            auto const stamp = static_cast<long long>(std::time(nullptr));
            auto const path = dir / ("benchmark_" + std::to_string(stamp) + ".csv");
            std::ofstream out(path, std::ios::binary);
            if (!out) {
                log::warn("Minimum: could not write the benchmark to {}", path.string());
                notify("Benchmark could not be saved, see the Geode log", NotificationIcon::Warning);
                return;
            }
            out << g_recorder.csv();
            out.close();

            log::info("Minimum: benchmark saved to {} ({} frames)", path.string(), rows);
            notify(fmt::format("Benchmark saved: {} frames", rows), NotificationIcon::Success);
        }

    }

    void noteSecretTap(double nowSec) {
        if (g_recorder.active()) {
            g_recorder.stop();
            saveRecording();
            return;
        }
        if (!g_taps.tap(nowSec)) return;

        g_recorder.start(nowSec, kBenchmarkSec);
        notify("Benchmark started, 60 seconds", NotificationIcon::Info);
    }

    void noteBenchmarkFrame(double nowSec, double frameMs, double cpuMs) {
        if (!g_recorder.active()) return;
        if (g_recorder.add(nowSec, frameMs, cpuMs)) {
            saveRecording();
        }
    }

    double benchmarkRemainingSec(double nowSec) {
        return g_recorder.remainingSec(nowSec);
    }

}
