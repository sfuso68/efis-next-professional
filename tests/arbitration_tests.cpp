// Prove dell'arbitrato fra sorgenti.
//
// Hanno un eseguibile proprio perche' verificano una categoria a se': non il
// calcolo di una grandezza, ma quale sorgente la fornisce e cosa accade quando
// smette di farlo. Tenerle separate fa vedere subito quale delle due e' caduta.

#include "efis/core/flight_model.hpp"
#include "efis/core/snapshot_arbitrator.hpp"
#include "efis/io/byte_source.hpp"
#include "efis/providers/mavlink_adapter.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

efis::Sample<double> reading(double value, efis::SourceId source,
                             efis::SignalId signal, efis::Validity validity,
                             efis::MonoTime at) {
    efis::Sample<double> sample{};
    sample.value = value;
    sample.raw = value;
    sample.source = source;
    sample.sampled_at = at;
    sample.received_at = at;
    sample.quality = efis::Quality::nominal;
    sample.validity = validity;
    sample.origin.signal = signal;
    sample.origin.unit = efis::Unit::degree;
    return sample;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};
    const auto now = epoch + 1s;

    // --- Arbitrato per grandezza, sui dati reali -----------------------------
    //
    // E' la prova che conta. Il Pixhawk della registrazione fornisce un assetto
    // valido ma nessuna posizione, perche' il satellitare non ha il fix, e
    // nessuna velocita' indicata, perche' dichiara non sano il sensore di
    // pressione differenziale. La simulazione fornisce tutto.
    //
    // Scegliere una sorgente in blocco vorrebbe dire o buttare l'assetto vero
    // per avere una posizione, o mostrare un aeroplano senza posizione ne'
    // velocita'. Arbitrare per grandezza permette la terza risposta, che e'
    // l'unica corretta: assetto dal Pixhawk, il resto dalla simulazione.
    const std::filesystem::path capture =
        std::filesystem::path{EFIS_TEST_DATA_DIR} / "mavlink" /
        "arduplane-4.7.0-usb-20260821.mav";
    require(std::filesystem::exists(capture), "the reference capture must be available");

    efis::MavlinkAdapter pixhawk{};
    efis::io::RecordedByteSource replay{capture};
    efis::io::pump(replay, pixhawk, now);

    efis::SimulationProvider simulator{42};
    const auto simulated = simulator.sample(now, 1s);

    // Il Pixhawk ha priorita': e' il primo dell'elenco.
    std::array<efis::SnapshotContribution, 2> sources{
        efis::SnapshotContribution{efis::SourceId::pixhawk_primary, pixhawk.snapshot()},
        efis::SnapshotContribution{efis::SourceId::simulation, simulated}};

    efis::SnapshotArbitrator arbitrator{{.reversion_dwell = 2000ms}};
    const auto combined = arbitrator.combine(sources, now);

    require(arbitrator.selected_for(efis::SignalId::roll) ==
                efis::SourceId::pixhawk_primary,
            "attitude must come from the flight controller, which provides it");
    require(combined.attitude.roll_deg.source == efis::SourceId::pixhawk_primary,
            "the arbitrated sample must carry the source it came from");
    require(!combined.attitude.roll_deg.is_simulated(),
            "an attitude taken from the flight controller is not simulated");

    require(arbitrator.selected_for(efis::SignalId::latitude) ==
                efis::SourceId::simulation,
            "without a fix the position must fall back to the next source");
    require(combined.position.latitude_deg.is_simulated(),
            "a position that fell back to simulation must say so");
    require(combined.position.latitude_deg.usable(),
            "the fallback position must be usable, otherwise the failover is pointless");

    require(arbitrator.selected_for(efis::SignalId::indicated_airspeed) ==
                efis::SourceId::simulation,
            "an airspeed from a sensor declared unhealthy must be replaced");

    // Due grandezze, due sorgenti, nello stesso istante: e' il guasto parziale.
    require(arbitrator.selected_for(efis::SignalId::roll) !=
                arbitrator.selected_for(efis::SignalId::latitude),
            "different signals must be free to come from different sources");

    require(arbitrator.statistics().mislabelled_samples == 0,
            "no producer must be putting a signal in the wrong field");

    // Ogni prima selezione e' un evento: la black box deve poter ricostruire da
    // dove veniva ciascuna grandezza fin dall'inizio, non solo dopo il primo
    // guasto.
    require(arbitrator.events().size() == 18,
            "every arbitrated signal must announce its initial source");
    require(arbitrator.events().front().reason == efis::SourceChangeReason::initial,
            "the first selection must be recorded as initial, not as a failover");

    // L'arbitro non calcola salute ne' allarmi: lo fa il modello di volo, subito
    // dopo. Il confronto fra le due catene mostra a cosa serve l'arbitrato.
    //
    // Il solo Pixhawk lascia scoperte posizione, velocita' al suolo e velocita'
    // indicata: il modello di volo dichiara warning, ed e' corretto, perche' un
    // aeroplano senza posizione ne' velocita' non e' in condizioni normali.
    // Non const: su questo ramo il modello di volo tiene stato interno per la
    // sorveglianza della stasi e il confronto incrociato.
    efis::FlightModel model{500ms};
    const auto pixhawk_alone = model.normalize(pixhawk.snapshot(), now);
    require(pixhawk_alone.health.unavailable_fields > 0,
            "the flight controller alone leaves fields unavailable");
    require(pixhawk_alone.health.level == efis::HealthLevel::warning,
            "unavailable fields must produce a warning, not a normal state");

    // Con il ripiego quelle grandezze tornano coperte, e la salute torna
    // normale: non perche' il guasto sia sparito, ma perche' una sorgente
    // utilizzabile e' stata trovata per ciascuna. Che sia simulata resta scritto
    // nei campioni, dove nessuno puo' scambiarla per reale.
    const auto arbitrated = model.normalize(combined, now);
    require(arbitrated.health.unavailable_fields == 0,
            "arbitration must leave no field uncovered");
    require(arbitrated.health.level == efis::HealthLevel::normal,
            "with every signal served the state is normal");
    require(arbitrated.position.latitude_deg.is_simulated(),
            "the covered position must still declare that it is simulated");

    // --- Failover e ritorno --------------------------------------------------
    efis::SnapshotArbitrator failover{{.reversion_dwell = 2000ms}};
    auto primary = efis::FlightSnapshot{};
    auto secondary = efis::FlightSnapshot{};

    primary.attitude.roll_deg = reading(10.0, efis::SourceId::pixhawk_primary,
                                        efis::SignalId::roll, efis::Validity::valid, now);
    secondary.attitude.roll_deg = reading(20.0, efis::SourceId::simulation,
                                          efis::SignalId::roll,
                                          efis::Validity::simulated, now);
    std::array<efis::SnapshotContribution, 2> pair{
        efis::SnapshotContribution{efis::SourceId::pixhawk_primary, primary},
        efis::SnapshotContribution{efis::SourceId::simulation, secondary}};

    auto first = failover.combine(pair, now);
    require(first.attitude.roll_deg.value == 10.0, "the preferred source must win");
    require(failover.statistics().failovers == 0, "an initial choice is not a failover");

    // La sorgente attiva cade: il passaggio deve essere immediato, senza attesa.
    // Aspettare qui significherebbe mostrare un dato non valido nel frattempo.
    pair[0].snapshot.attitude.roll_deg.validity = efis::Validity::unavailable;
    const auto fallen = failover.combine(pair, now + 100ms);
    require(fallen.attitude.roll_deg.value == 20.0,
            "the loss of the active source must switch immediately");
    require(failover.statistics().failovers == 1, "the failover must be counted");
    require(failover.selected_for(efis::SignalId::roll) == efis::SourceId::simulation,
            "the arbitrator must report the source actually in use");

    // La sorgente preferita torna. Il ritorno non e' immediato: una sorgente che
    // oscilla farebbe rimbalzare la grandezza fra due valori, e un valore che
    // salta e' peggio di un valore che resta sul ripiego ancora un momento.
    pair[0].snapshot.attitude.roll_deg.validity = efis::Validity::valid;
    const auto too_soon = failover.combine(pair, now + 200ms);
    require(too_soon.attitude.roll_deg.value == 20.0,
            "a source that has just returned must not be trusted at once");
    require(failover.statistics().reversions == 0, "no reversion may have happened yet");

    const auto after_dwell = failover.combine(pair, now + 3s);
    require(after_dwell.attitude.roll_deg.value == 10.0,
            "after the dwell time the preferred source must be taken back");
    require(failover.statistics().reversions == 1, "the reversion must be counted");

    // Un'oscillazione non deve produrre un ritorno: il cronometro riparte.
    pair[0].snapshot.attitude.roll_deg.validity = efis::Validity::unavailable;
    const auto dropped_again = failover.combine(pair, now + 4s);
    require(dropped_again.attitude.roll_deg.value == 20.0,
            "losing the preferred source again must fall back at once");
    pair[0].snapshot.attitude.roll_deg.validity = efis::Validity::valid;
    const auto blinked = failover.combine(pair, now + 4s + 100ms);
    require(blinked.attitude.roll_deg.value == 20.0,
            "a source back for a moment has not yet earned the dwell time");
    pair[0].snapshot.attitude.roll_deg.validity = efis::Validity::unavailable;
    const auto flapping = failover.combine(pair, now + 4s + 200ms);
    require(flapping.attitude.roll_deg.value == 20.0,
            "a flapping source must not be taken back before it settles");

    // --- Nessuna sorgente utilizzabile ---------------------------------------
    //
    // Il caso che conta di piu': quando cadono tutte, non si mostra l'ultimo
    // valore buono come se fosse attuale. Il paragrafo 22 dell'handoff lo
    // chiama mascherare un errore con un valore plausibile.
    efis::SnapshotArbitrator blackout{};
    std::array<efis::SnapshotContribution, 1> only{
        efis::SnapshotContribution{efis::SourceId::pixhawk_primary, primary}};
    only[0].snapshot.attitude.roll_deg =
        reading(33.0, efis::SourceId::pixhawk_primary, efis::SignalId::roll,
                efis::Validity::valid, now);
    const auto lit = blackout.combine(only, now);
    require(lit.attitude.roll_deg.value == 33.0, "the only usable source must be taken");

    only[0].snapshot.attitude.roll_deg.validity = efis::Validity::unavailable;
    const auto dark = blackout.combine(only, now + 1s);
    require(!dark.attitude.roll_deg.usable(),
            "with every source down the value must not be usable");
    require(dark.attitude.roll_deg.validity == efis::Validity::unavailable,
            "with every source down the field must be declared unavailable");

    // --- Campione etichettato male -------------------------------------------
    //
    // Una grandezza finita nel campo sbagliato e' un difetto di chi la produce.
    // Viene contata e non corretta: riscrivere l'etichetta la nasconderebbe e
    // renderebbe la registrazione una descrizione falsa dell'accaduto.
    efis::SnapshotArbitrator labelling{};
    std::array<efis::SnapshotContribution, 1> wrong{
        efis::SnapshotContribution{efis::SourceId::pixhawk_primary, primary}};
    wrong[0].snapshot.attitude.roll_deg =
        reading(5.0, efis::SourceId::pixhawk_primary, efis::SignalId::pitch,
                efis::Validity::valid, now);
    const auto mislabelled = labelling.combine(wrong, now);
    require(labelling.statistics().mislabelled_samples == 1,
            "a signal placed in the wrong field must be counted");
    require(mislabelled.attitude.roll_deg.origin.signal == efis::SignalId::pitch,
            "the wrong label must be preserved, not silently rewritten");

    // Gli eventi si consumano: la black box li preleva e l'arbitro riparte.
    require(!labelling.events().empty(), "the initial selection produced an event");
    labelling.clear_events();
    require(labelling.events().empty(), "collected events must be released");

    std::cout << "All EFIS arbitration tests passed\n";
}
