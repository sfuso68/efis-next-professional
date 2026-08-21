// Produce quadranti in SVG per poterli guardare.
//
// Serve mentre gli strumenti si stanno ancora scrivendo: un quadrante puo'
// essere geometricamente corretto e illeggibile, e l'unico modo per accorgersene
// e' vederlo. Gli stessi file diventeranno le immagini di confronto delle prove.
//
// I valori vengono dai simulatori di volo e di motore gia' in funzione, non da
// numeri scelti per far bella figura: e' cio' che il pannello mostrerebbe adesso.

#include "efis/core/signal_access.hpp"
#include "efis/instruments/dial.hpp"
#include "efis/instruments/svg_dial_renderer.hpp"
#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace efis;
using namespace efis::instruments;

void write(const std::filesystem::path& directory, const std::string& name,
           const Dial& dial, const DialStyle& style) {
    const auto path = directory / (name + ".svg");
    std::ofstream out{path};
    out << render_svg(dial, style);
    std::cout << "  " << path.filename().string() << "\n";
}

Reading from(const Sample<double>* sample) {
    if (sample == nullptr) return {0.0, Validity::unavailable, false};
    return {sample->value, sample->validity, sample->is_simulated()};
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path directory = argc > 1 ? argv[1] : "output/dials";
    std::filesystem::create_directories(directory);

    using namespace std::chrono_literals;
    const auto now = MonoTime{} + 40s;
    const auto flight = SimulationProvider{42}.sample(now, 40s);
    const auto engine = EngineSimulationProvider{84}.sample(now, 40s);

    std::cout << "Quadranti scritti in " << directory.string() << ":\n";

    // Anemometro con le fasce del profilo LSA gia' attivo: VSO 39, VS1 45,
    // VFE 85, VNO 120, VNE 140.
    const Scale asi_scale{40.0, 160.0, -150.0, 300.0, 20.0, 5.0, 0, 0.0};
    const std::vector<BandRange> asi_bands{
        {39.0, 85.0, Band::white},
        {45.0, 120.0, Band::green},
        {120.0, 140.0, Band::yellow},
        {140.0, 160.0, Band::red}};
    const auto airspeed = from(find_signal(flight, SignalId::indicated_airspeed));

    write(directory, "01-anemometro-480",
          describe(asi_scale, airspeed, Sizing{480.0}, asi_bands),
          DialStyle{"IAS", "KT", 480.0});

    // Lo stesso strumento su un display circolare piccolo: le tacche si
    // diradano da sole, la lettura resta.
    write(directory, "02-anemometro-240",
          describe(asi_scale, airspeed, Sizing{240.0}, asi_bands),
          DialStyle{"IAS", "KT", 240.0});

    // Contagiri con i limiti del Rotax 912 ULS.
    const Scale rpm_scale{0.0, 6000.0, -150.0, 300.0, 1000.0, 200.0, 0, 0.0};
    const std::vector<BandRange> rpm_bands{
        {1400.0, 5500.0, Band::green},
        {5500.0, 5800.0, Band::yellow},
        {5800.0, 6000.0, Band::red}};
    write(directory, "03-contagiri-480",
          describe(rpm_scale, from(find_signal(engine, SignalId::engine_rpm)),
                   Sizing{480.0}, rpm_bands),
          DialStyle{"RPM", "1/min", 480.0});

    // Pressione olio.
    const Scale oil_scale{0.0, 120.0, -150.0, 300.0, 20.0, 5.0, 0, 0.0};
    const std::vector<BandRange> oil_bands{
        {0.0, 12.0, Band::red},
        {12.0, 100.0, Band::green},
        {100.0, 120.0, Band::red}};
    write(directory, "04-pressione-olio-480",
          describe(oil_scale, from(find_signal(engine, SignalId::oil_pressure)),
                   Sizing{480.0}, oil_bands),
          DialStyle{"OIL P", "PSI", 480.0});

    // Variometro: scala centrata sullo zero, che e' il caso in cui la lancetta
    // deve poter andare da entrambe le parti.
    const Scale vsi_scale{-2000.0, 2000.0, -150.0, 300.0, 500.0, 100.0, 0, 0.0};
    write(directory, "05-variometro-480",
          describe(vsi_scale, from(find_signal(flight, SignalId::vertical_speed)),
                   Sizing{480.0}),
          DialStyle{"VSI", "FT/MIN", 480.0});

    // I tre casi che distinguono uno strumento onesto da uno decorativo.
    write(directory, "06-guasto-480",
          describe(asi_scale, {0.0, Validity::unavailable, false}, Sizing{480.0}, asi_bands),
          DialStyle{"IAS", "KT", 480.0});

    write(directory, "07-scaduto-480",
          describe(asi_scale, {95.0, Validity::stale, false}, Sizing{480.0}, asi_bands),
          DialStyle{"IAS", "KT", 480.0});

    write(directory, "08-fuori-scala-480",
          describe(asi_scale, {195.0, Validity::valid, false}, Sizing{480.0}, asi_bands),
          DialStyle{"IAS", "KT", 480.0});

    // Altimetro a tre lancette, con la quota indicata e la regolazione
    // effettivamente in uso: entrambe vengono dal core, non da qui.
    const auto indicated = from(find_signal(flight, SignalId::indicated_altitude));
    const auto* setting = find_signal(flight, SignalId::baro_setting);
    const std::string setting_text =
        setting != nullptr && setting->usable()
            ? std::to_string(static_cast<int>(setting->value + 0.5))
            : std::string{};

    write(directory, "09-altimetro-480",
          describe_altimeter({indicated, setting_text}, Sizing{480.0}),
          DialStyle{"ALT", "FT", 480.0});

    write(directory, "10-altimetro-240",
          describe_altimeter({indicated, setting_text}, Sizing{240.0}),
          DialStyle{"ALT", "FT", 240.0});

    // Le lancette si avvolgono e non hanno estremi: un dato corrotto
    // mostrerebbe una quota bassa credibile. Fuori dai limiti non si indica.
    write(directory, "11-altimetro-implausibile-480",
          describe_altimeter({{1000000.0, Validity::valid, false}, setting_text},
                             Sizing{480.0}),
          DialStyle{"ALT", "FT", 480.0});

    // Girodirezionale con la prua del simulatore e la prua selezionata.
    const auto course = from(find_signal(flight, SignalId::selected_course));
    write(directory, "12-girodirezionale-480",
          describe_heading({from(find_signal(flight, SignalId::heading)), course},
                           Sizing{480.0}),
          DialStyle{"HDG", "DEG", 480.0});

    write(directory, "13-girodirezionale-240",
          describe_heading({from(find_signal(flight, SignalId::heading)), course},
                           Sizing{240.0}),
          DialStyle{"HDG", "DEG", 240.0});

    // Giroscopio perduto: nessuna rosa, resta la linea di fede e cio' che il
    // pilota aveva selezionato.
    write(directory, "14-girodirezionale-guasto-480",
          describe_heading({{0.0, Validity::unavailable, false}, course}, Sizing{480.0}),
          DialStyle{"HDG", "DEG", 480.0});

    return 0;
}
