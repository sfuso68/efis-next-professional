// Osservatore del bus CAN del nodo sensori.
//
// Apre un'interfaccia SocketCAN, interpreta i frame con l'adapter e stampa cio'
// che ne esce. E' lo strumento con cui il trasporto si prova sul bus vero, e la
// ragione per cui esiste: fino a qui la catena CAN era verificata su
// registrazioni, che dimostrano la conformita' alla specifica e non che il bus
// si legga davvero.
//
// Sola lettura. Non trasmette nulla, nemmeno sul bus virtuale.

#include "efis/io/socketcan_source.hpp"
#include "efis/providers/esp32_can_adapter.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic_bool running{true};

void stop(int) { running.store(false); }

const char* validity_text(efis::Validity validity) {
    switch (validity) {
        case efis::Validity::valid: return "VALIDO";
        case efis::Validity::degraded: return "DEGRADATO";
        case efis::Validity::stale: return "SCADUTO";
        case efis::Validity::unavailable: return "NON DISPONIBILE";
        case efis::Validity::out_of_range: return "FUORI SCALA";
        case efis::Validity::disagree: return "IN DISACCORDO";
        case efis::Validity::simulated: return "SIMULATO";
    }
    return "NON DISPONIBILE";
}

void print_row(const char* name, const efis::Sample<double>& sample) {
    if (sample.received_at == efis::MonoTime{}) {
        std::printf("  %-22s  %-12s %s\n", name, "———", "mai ricevuto");
        return;
    }
    // Un valore non utilizzabile non mostra il numero: il motivo prende il suo
    // posto, come nel banco prova del Setup Center.
    if (sample.usable()) {
        std::printf("  %-22s  %12.2f %-6s %s\n", name, sample.value,
                    efis::unit_symbol(sample.origin.unit), validity_text(sample.validity));
    } else {
        std::printf("  %-22s  %-12s %-6s %s\n", name, "———",
                    efis::unit_symbol(sample.origin.unit), validity_text(sample.validity));
    }
}

}  // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);

    const std::string interface_name = argc > 1 ? argv[1] : "vcan0";
    const int seconds = argc > 2 ? std::atoi(argv[2]) : 0;

    if (!efis::io::SocketCanSource::supported()) {
        std::cerr << "event=can_monitor_unsupported piattaforma senza SocketCAN\n";
        return 2;
    }

    efis::io::SocketCanSource source{interface_name};
    if (!source.open()) {
        // Un'interfaccia che non si apre non e' un bus silenzioso, ed e' la
        // distinzione che questo programma esiste per rendere visibile.
        std::cerr << "event=can_monitor_open_failed interface=" << interface_name
                  << " reason=\"" << source.error() << "\"\n";
        return 1;
    }

    std::cout << "event=can_monitor_start interface=" << interface_name
              << " mode=READ_ONLY\n";

    efis::Esp32CanAdapter adapter;
    std::vector<efis::can::Frame> buffer(64);

    const auto started = efis::MonoClock::now();
    auto next_report = started;

    while (running.load()) {
        const auto now = efis::MonoClock::now();
        const std::size_t read = source.read(buffer);
        if (read > 0) {
            adapter.consume(std::span<const efis::can::Frame>{buffer.data(), read}, now);
        }
        adapter.update(now);

        if (now >= next_report) {
            next_report = now + 1s;
            const auto& statistics = source.statistics();
            const auto& state = adapter.sensor_state();

            std::printf("\n--- %s ---\n", interface_name.c_str());
            std::printf("  bus: %llu frame, %llu errori, %llu remoti, %llu persi dal kernel\n",
                        static_cast<unsigned long long>(statistics.frames_read),
                        static_cast<unsigned long long>(statistics.error_frames),
                        static_cast<unsigned long long>(statistics.remote_frames),
                        static_cast<unsigned long long>(statistics.dropped_by_kernel));
            std::printf("  adapter: %llu applicati, %llu ignorati, %llu troncati\n",
                        static_cast<unsigned long long>(adapter.statistics().frames_applied),
                        static_cast<unsigned long long>(adapter.statistics().frames_ignored),
                        static_cast<unsigned long long>(adapter.statistics().frames_truncated));
            std::printf("  calibrazione: sistema %u magnetometro %u  sensori: imu %d baro %d pitot %d\n",
                        state.calibration_system, state.calibration_magnetometer,
                        state.imu_ok ? 1 : 0, state.barometer_ok ? 1 : 0,
                        state.pitot_ok ? 1 : 0);
            std::printf("  satellitare: fix %d, %u satelliti\n",
                        state.gnss_fix ? 1 : 0, state.gnss_satellites);

            print_row("rollio", adapter.snapshot().attitude.roll_deg);
            print_row("beccheggio", adapter.snapshot().attitude.pitch_deg);
            print_row("prua", adapter.snapshot().attitude.heading_deg);
            print_row("velocita indicata", adapter.snapshot().air_data.indicated_airspeed_kt);
            print_row("quota", adapter.snapshot().air_data.pressure_altitude_ft);
            print_row("pressione statica", state.static_pressure_hpa);
            print_row("pressione differenziale", state.differential_pressure_pa);
            print_row("latitudine", adapter.snapshot().position.latitude_deg);
            std::fflush(stdout);
        }

        if (seconds > 0 && now - started >= std::chrono::seconds{seconds}) break;
        std::this_thread::sleep_for(10ms);
    }

    std::cout << "event=can_monitor_stop frames=" << source.statistics().frames_read
              << " applied=" << adapter.statistics().frames_applied << '\n';
    return 0;
}
