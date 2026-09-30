#include "efis/core/flight_model.hpp"
#include "efis/ingest/recording_session.hpp"
#include "efis/io/byte_source.hpp"
#include "efis/providers/simulation_provider.hpp"
#include "efis/recording/recorder_status.hpp"
#include "efis/recording/segmented_recorder.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

std::atomic_bool running{true};

void stop(int) { running.store(false); }

std::uint64_t wall_milliseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

struct Options final {
    std::filesystem::path output{"records"};
    std::filesystem::path status{"run/recorder.status"};

    // Registrazione MAVLink da rileggere. Quando e' vuota si registra dal
    // simulatore, che resta un ripiego dichiarato e non un dato reale.
    std::filesystem::path replay;
    efis::recording::RecorderSettings recorder;
};

std::uint64_t unsigned_option(const std::string& name, const char* value) {
    const std::string text{value};
    if (text.empty() || text.front() == '-') {
        throw std::invalid_argument(name + " requires an unsigned integer");
    }
    std::size_t parsed{};
    std::uint64_t result{};
    try {
        result = std::stoull(text, &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument(name + " requires an unsigned integer");
    }
    if (parsed != text.size()) {
        throw std::invalid_argument(name + " requires an unsigned integer");
    }
    return result;
}

Options parse(int argc, char** argv) {
    Options options;
    int positional = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        if (argument == "--replay" && index + 1 < argc) {
            options.replay = argv[++index];
        } else if (argument == "--max-storage-bytes" && index + 1 < argc) {
            options.recorder.max_completed_bytes =
                unsigned_option(argument, argv[++index]);
        } else if (argument == "--min-free-bytes" && index + 1 < argc) {
            options.recorder.min_free_bytes = unsigned_option(argument, argv[++index]);
        } else if (argument == "--minimum-completed-segments" && index + 1 < argc) {
            const auto value = unsigned_option(argument, argv[++index]);
            if (value > std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument(argument + " exceeds platform size");
            }
            options.recorder.minimum_completed_segments =
                static_cast<std::size_t>(value);
        } else if (argument.starts_with("--")) {
            throw std::invalid_argument("unknown or incomplete recorder option: " + argument);
        } else if (positional == 0) {
            options.output = argument;
            ++positional;
        } else if (positional == 1) {
            options.status = argument;
            ++positional;
        }
    }
    return options;
}

efis::recording::RecorderStatus status_for(
    const efis::recording::SegmentedRecorder& recorder,
    std::string state,
    std::string segment = {}) {
    return {
        .state = std::move(state),
        .updated_wall_ms = wall_milliseconds(),
        .records = recorder.records_written(),
        .segment = std::move(segment),
        .write_failed = !recorder.healthy(),
        .last_error = recorder.last_error(),
        .retention_enabled = recorder.retention_enabled(),
        .completed_segments = recorder.completed_segments(),
        .completed_bytes = recorder.completed_bytes(),
        .available_bytes = recorder.available_bytes(),
        .pruned_segments = recorder.pruned_segments(),
        .pruned_bytes = recorder.pruned_bytes()};
}

}  // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    Options options;
    std::unique_ptr<efis::recording::SegmentedRecorder> recorder;
    std::uint64_t count{};
    try {
        options = parse(argc, argv);
        recorder = std::make_unique<efis::recording::SegmentedRecorder>(
            options.output, options.recorder);

        const auto started = efis::MonoClock::now();
        auto next = started;

    // Due modi di registrare, e la differenza non e' un dettaglio di comodo: uno
    // porta nella black box quello che il controllore ha davvero detto, l'altro
    // porta numeri costruiti. Il modo e' dichiarato nel log all'avvio, perche'
    // rileggendo una registrazione si deve poter sapere che cosa si sta
    // leggendo. I campioni simulati restano marcati tali anche dentro il
    // registro, quindi la distinzione sopravvive al file.
    if (!options.replay.empty()) {
        efis::io::RecordedByteSource source{options.replay};
        efis::ingest::RecordingSession session{*recorder};

        std::cout << "event=recorder_start mode=MAVLINK_REPLAY source="
                  << options.replay.string() << " bytes=" << source.size()
                  << " directory=" << options.output.string()
                  << " recovered=" << recorder->recovered_segments() << '\n';

        while (running.load() && !source.exhausted()) {
            const auto now = efis::MonoClock::now();
            session.step(source, now);
            ++count;
            if (count == 1 || count % 50 == 0) {
                efis::recording::write_recorder_status(
                    options.status,
                    status_for(*recorder, "RECORDING",
                               recorder->current_segment_name()));
            }
            next += 20ms;
            std::this_thread::sleep_until(next);
        }

        const auto& statistics = session.statistics();
        std::cout << "event=replay_complete bytes=" << statistics.bytes_read
                  << " snapshots=" << statistics.snapshots_recorded
                  << " node_events=" << statistics.node_events_recorded
                  << " source_events=" << statistics.source_events_recorded
                  << " frames=" << session.pipeline().adapter().statistics().frames_applied
                  << '\n';
    } else {
        efis::SimulationProvider provider{42};
        efis::FlightModel model;

        std::cout << "event=recorder_start mode=SIM directory="
                  << options.output.string()
                  << " recovered=" << recorder->recovered_segments() << '\n';

        while (running.load()) {
            const auto now = efis::MonoClock::now();
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - started);
            recorder->append(model.normalize(provider.sample(now, elapsed), now));
            ++count;
            if (count == 1 || count % 50 == 0) {
                efis::recording::write_recorder_status(
                    options.status,
                    status_for(*recorder, "RECORDING",
                               recorder->current_segment_name()));
            }
            next += 20ms;
            std::this_thread::sleep_until(next);
        }
    }

        recorder->close();
        efis::recording::write_recorder_status(
            options.status, status_for(*recorder, "STOPPED"));
        std::cout << "event=recorder_stop records=" << recorder->records_written()
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        const auto records = recorder ? recorder->records_written() : 0;
        const auto segment = recorder ? recorder->current_segment_name() : std::string{};
        try {
            auto status = efis::recording::RecorderStatus{
                .state = "ERROR", .updated_wall_ms = wall_milliseconds(),
                .records = records, .segment = segment,
                .write_failed = true,
                .last_error = error.what()};
            if (recorder) {
                status.retention_enabled = recorder->retention_enabled();
                status.completed_segments = recorder->completed_segments();
                status.completed_bytes = recorder->completed_bytes();
                status.available_bytes = recorder->available_bytes();
                status.pruned_segments = recorder->pruned_segments();
                status.pruned_bytes = recorder->pruned_bytes();
            }
            efis::recording::write_recorder_status(options.status, status);
        } catch (const std::exception& status_error) {
            std::cerr << "event=recorder_status_failed reason=\""
                      << status_error.what() << "\"\n";
        }
        std::cerr << "event=recorder_failed records=" << records
                  << " reason=\"" << error.what() << "\"\n";
        return 2;
    }
}
