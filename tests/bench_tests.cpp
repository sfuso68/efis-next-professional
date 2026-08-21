// Prove del banco prova sensori, passo 6 del backbone dati P1.
//
// Il banco viene alimentato dalla registrazione reale acquisita dal Pixhawk 6X
// il 21 agosto 2026 in ascolto passivo. Non e' un dettaglio di comodo: le
// condizioni che contano, satellitare senza fix e sensore differenziale
// dichiarato non sano, sono quelle vere del banco, e sono esattamente quelle in
// cui un cruscotto scritto con leggerezza mostrerebbe numeri rassicuranti.

#include "efis/bench/sensor_bench.hpp"
#include "efis/io/byte_source.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

int failures = 0;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};

    const std::filesystem::path capture =
        std::filesystem::path{EFIS_TEST_DATA_DIR} / "mavlink" /
        "arduplane-4.7.0-usb-20260821.mav";
    require(std::filesystem::exists(capture),
            "the reference capture must be available to the bench tests");
    if (!std::filesystem::exists(capture)) return EXIT_FAILURE;

    efis::bench::SensorBench bench{
        std::make_unique<efis::io::RecordedByteSource>(capture)};

    auto now = epoch + 1s;
    while (!bench.exhausted()) {
        bench.pump(now);
        now += 20ms;
    }
    bench.pump(now);

    const auto& view = bench.view();

    require(view.receiving(), "the bench must report that frames are arriving");
    require(view.frames > 2000, "the whole capture must have been consumed");
    require(view.crc_errors == 0, "a clean recording must not produce checksum errors");

    // --- La regola che il banco esiste per rendere visibile -------------------
    require(!view.feeds_flight_control,
            "nothing on this bench may feed flight control while ARSPD_USE is zero");

    // --- Assetto: presente e utilizzabile ------------------------------------
    const auto* roll = view.find(efis::SignalId::roll);
    require(roll != nullptr, "attitude must appear on the bench");
    require(roll != nullptr && roll->usable,
            "attitude arrives continuously and must be usable");
    require(roll != nullptr && roll->unit == efis::Unit::degree,
            "attitude must declare degrees");
    require(roll != nullptr && !roll->simulated,
            "data from the real autopilot must not be marked simulated");

    // --- Satellitare: comunicante, senza fix ---------------------------------
    //
    // E' lo stato reale del banco, e il caso in cui un cruscotto ingenuo
    // mostrerebbe latitudine e longitudine pari a zero come se fossero un punto
    // sulla carta, al largo dell'Africa occidentale.
    require(view.gnss_fix_type == 1, "the recorded GNSS state is communicating without fix");
    require(view.gnss_satellites == 0, "the recorded GNSS state reports no satellites");

    const auto* latitude = view.find(efis::SignalId::latitude);
    require(latitude != nullptr, "the position must appear, so that it can be shown unusable");
    require(latitude != nullptr && !latitude->usable,
            "without a three dimensional fix the position must not be usable");
    require(latitude != nullptr && latitude->note == efis::bench::BenchNote::no_gnss_fix,
            "the bench must say why the position is unusable, not merely dim it");
    require(std::string{efis::bench::bench_note_text(efis::bench::BenchNote::no_gnss_fix)} ==
                "nessun fix tridimensionale",
            "the reason must be readable, not a code");

    const auto* ground_speed = view.find(efis::SignalId::ground_speed);
    require(ground_speed != nullptr && !ground_speed->usable,
            "ground speed derives from the same fix and must follow it");

    // --- Sensore aria: misura, ma il volo non lo usa -------------------------
    //
    // Il sensore e' presente e trasmette, e l'autopilota lo dichiara non sano
    // perche' non e' abilitato al controllo di volo. La pressione differenziale
    // resta osservabile, ed e' il dato di cui il sensore e' realmente portatore;
    // la velocita' che se ne ricava no.
    require(view.air_data_present, "the differential pressure sensor is present");
    require(!view.air_data_healthy,
            "the recorded state has the sensor declared unhealthy");

    const auto* pressure = view.find(efis::SignalId::differential_pressure);
    require(pressure != nullptr, "the raw differential pressure must appear on the bench");
    require(pressure != nullptr && pressure->usable,
            "the raw measurement stays observable: it is what the sensor actually measures");
    require(pressure != nullptr && pressure->unit == efis::Unit::hectopascal,
            "the differential pressure must declare hectopascals");

    const auto* temperature = view.find(efis::SignalId::air_data_temperature);
    require(temperature != nullptr, "the element temperature must appear on the bench");
    require(temperature != nullptr && temperature->value > 30.0 &&
                temperature->value < 40.0,
            "the recorded element temperature was about 33.85 degrees");

    const auto* airspeed = view.find(efis::SignalId::indicated_airspeed);
    require(airspeed != nullptr, "the computed airspeed must appear, to be shown unusable");
    require(airspeed != nullptr && !airspeed->usable,
            "a sensor declared unhealthy must not yield a usable airspeed");
    require(airspeed != nullptr &&
                airspeed->note == efis::bench::BenchNote::sensor_declared_unhealthy,
            "the bench must say that the sensor itself declared the problem");

    // --- Eta' dei valori ------------------------------------------------------
    //
    // Il silenzio deve far invecchiare le righe. Senza questo, un banco lasciato
    // aperto dopo lo spegnimento del Pixhawk continuerebbe a mostrare l'ultimo
    // assetto come se fosse quello attuale.
    bench.pump(now + 10s);
    const auto* aged = bench.view().find(efis::SignalId::roll);
    require(aged != nullptr && !aged->usable,
            "silence must make a row unusable, not leave the last value standing");
    require(aged != nullptr && aged->note == efis::bench::BenchNote::stale,
            "an aged row must say that the value is stale");
    require(aged != nullptr && aged->age > 9s,
            "the bench must report how old the value is");

    // --- Una sorgente vuota non inventa nulla --------------------------------
    efis::bench::SensorBench empty{
        std::make_unique<efis::io::RecordedByteSource>(std::vector<std::uint8_t>{})};
    empty.pump(epoch + 1s);
    require(empty.view().rows.empty(),
            "with no data the bench must show nothing, not zeroed rows");
    require(!empty.view().receiving(),
            "with no data the bench must not claim to be receiving");
    require(!empty.view().feeds_flight_control,
            "an empty bench still declares that it feeds nothing");

    if (failures != 0) {
        std::cerr << failures << " bench checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All EFIS bench tests passed\n";
    return EXIT_SUCCESS;
}
