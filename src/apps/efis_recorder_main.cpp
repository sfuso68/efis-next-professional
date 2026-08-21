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
#include <memory>
#include <string>
#include <thread>

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
};

Options parse(int argc, char** argv) {
    Options options;
    int positional = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        if (argument == "--replay" && index + 1 < argc) {
            options.replay = argv[++index];
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

}  // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    const Options options = parse(argc, argv);
    efis::recording::SegmentedRecorder recorder{options.output};

    const auto started = efis::MonoClock::now();
    auto next = started;
    std::uint64_t count{};

    // Due modi di registrare, e la differenza non e' un dettaglio di comodo: uno
    // porta nella black box quello che il controllore ha davvero detto, l'altro
    // porta numeri costruiti. Il modo e' dichiarato nel log all'avvio, perche'
    // rileggendo una registrazione si deve poter sapere che cosa si sta
    // leggendo. I campioni simulati restano marcati tali anche dentro il
    // registro, quindi la distinzione sopravvive al file.
    if (!options.replay.empty()) {
        efis::io::RecordedByteSource source{options.replay};
        efis::ingest::RecordingSession session{recorder};

        std::cout << "event=recorder_start mode=MAVLINK_REPLAY source="
                  << options.replay.string() << " bytes=" << source.size()
                  << " directory=" << options.output.string()
                  << " recovered=" << recorder.recovered_segments() << '\n';

        while (running.load() && !source.exhausted()) {
            const auto now = efis::MonoClock::now();
            session.step(source, now);
            ++count;
            if (count == 1 || count % 50 == 0) {
                efis::recording::write_recorder_status(options.status, {
                    .state = "RECORDING",
                    .updated_wall_ms = wall_milliseconds(),
                    .records = count,
                    .segment = recorder.current_segment_name()});
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
                  << " recovered=" << recorder.recovered_segments() << '\n';

        while (running.load()) {
            const auto now = efis::MonoClock::now();
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - started);
            recorder.append(model.normalize(provider.sample(now, elapsed), now));
            ++count;
            if (count == 1 || count % 50 == 0) {
                efis::recording::write_recorder_status(options.status, {
                    .state = "RECORDING",
                    .updated_wall_ms = wall_milliseconds(),
                    .records = count,
                    .segment = recorder.current_segment_name()});
            }
            next += 20ms;
            std::this_thread::sleep_until(next);
        }
    }

    recorder.close();
    efis::recording::write_recorder_status(options.status, {
        .state = "STOPPED", .updated_wall_ms = wall_milliseconds(),
        .records = count, .segment = {}});
    std::cout << "event=recorder_stop records=" << count << '\n';
}
