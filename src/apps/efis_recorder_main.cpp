#include "efis/core/flight_model.hpp"
#include "efis/providers/simulation_provider.hpp"
#include "efis/recording/segmented_recorder.hpp"
#include "efis/recording/recorder_status.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {
std::atomic_bool running{true};
void stop(int) { running.store(false); }
std::uint64_t wall_milliseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
}

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    const std::filesystem::path output = argc > 1 ? argv[1] : "records";
    const std::filesystem::path status_path = argc > 2 ? argv[2] : "run/recorder.status";

    efis::SimulationProvider provider{42};
    efis::FlightModel model;
    efis::recording::SegmentedRecorder recorder{output};
    const auto started = efis::MonoClock::now();
    auto next = started;
    std::uint64_t count{};

    std::cout << "event=recorder_start mode=SIM directory=" << output.string()
              << " recovered=" << recorder.recovered_segments() << '\n';
    while (running.load()) {
        const auto now = efis::MonoClock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - started);
        recorder.append(model.normalize(provider.sample(now, elapsed), now));
        ++count;
        if (count == 1 || count % 50 == 0) {
            efis::recording::write_recorder_status(status_path, {
                .state = "RECORDING",
                .updated_wall_ms = wall_milliseconds(),
                .records = count,
                .segment = recorder.current_segment_name()});
        }
        next += 20ms;
        std::this_thread::sleep_until(next);
    }
    recorder.close();
    efis::recording::write_recorder_status(status_path, {
        .state = "STOPPED", .updated_wall_ms = wall_milliseconds(),
        .records = count, .segment = {}});
    std::cout << "event=recorder_stop records=" << count << '\n';
}
