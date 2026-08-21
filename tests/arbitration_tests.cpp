// Prove dell'arbitrato fra sorgenti.
//
// Hanno un eseguibile proprio perche' verificano una categoria a se': non il
// calcolo di una grandezza, ma quale sorgente la fornisce e cosa accade quando
// smette di farlo. Tenerle separate fa vedere subito quale delle due e' caduta.

#include "efis/core/flight_model.hpp"
#include "efis/core/engine_source.hpp"
#include "efis/core/flight_source.hpp"
#include "efis/core/snapshot_arbitrator.hpp"
#include "efis/io/byte_source.hpp"
#include "efis/providers/mavlink_adapter.hpp"
#include "efis/providers/simulation_provider.hpp"
#include "efis/providers/engine_simulation_source.hpp"
#include "efis/providers/simulation_source.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <memory>
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


// Sorgente finta che pubblica soltanto l'assetto, come farebbe un controllo di
// volo che non ha ancora il fix satellitare.
class AttitudeOnlySource final : public efis::FlightSource {
public:
    explicit AttitudeOnlySource(double roll) : roll_(roll) {}
    [[nodiscard]] efis::SourceId source() const noexcept override {
        return efis::SourceId::pixhawk_primary;
    }
    [[nodiscard]] efis::FlightSnapshot poll(efis::MonoTime now,
                                            std::chrono::milliseconds) override {
        efis::FlightSnapshot snapshot{};
        snapshot.attitude.roll_deg =
            reading(roll_, efis::SourceId::pixhawk_primary, efis::SignalId::roll,
                    efis::Validity::valid, now);
        return snapshot;
    }
    void set_roll(double roll) noexcept { roll_ = roll; }
    void fail() noexcept { roll_ = 0.0; failed_ = true; }
    [[nodiscard]] bool failed() const noexcept { return failed_; }

private:
    double roll_;
    bool failed_{};
};


// Nodo sensori motore finto: misura giri e temperatura testa del primo
// cilindro, come un modulo a cui siano stati collegati solo due canali.
class PartialEngineNode final : public efis::EngineSource {
public:
    [[nodiscard]] efis::SourceId source() const noexcept override {
        return efis::SourceId::esp32_legacy;
    }
    [[nodiscard]] efis::engine::EngineSnapshot poll(
        efis::MonoTime now, std::chrono::milliseconds) override {
        efis::engine::EngineSnapshot out{};
        out.rpm = reading(2500.0, efis::SourceId::esp32_legacy, efis::SignalId::engine_rpm,
                          rpm_valid_ ? efis::Validity::valid : efis::Validity::unavailable,
                          now);
        out.cht_c[0] = reading(191.0, efis::SourceId::esp32_legacy,
                               efis::SignalId::cylinder_head_temperature_1,
                               efis::Validity::valid, now);
        return out;
    }
    void lose_rpm() noexcept { rpm_valid_ = false; }

private:
    bool rpm_valid_{true};
};

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

    // --- Il ruolo della simulazione, ADR-003 ---------------------------------
    //
    // La simulazione non e' un residuo da togliere quando arriveranno i sensori
    // veri: e' cio' che permette di lavorare al banco quando i sensori non ci
    // sono, sono in parte collegati o si stanno tarando. Deve pero' essere una
    // scelta dichiarata, non un comportamento implicito.
    auto real = std::make_shared<AttitudeOnlySource>(7.5);
    auto sim = std::make_shared<efis::SimulationSource>(42);

    efis::FlightSourceRouter router{{.reversion_dwell = 2000ms}};
    router.add_source(real);
    router.set_simulation(sim);
    require(router.simulation_role() == efis::SimulationRole::fallback,
            "the default role must be fallback, never precedence over real data");

    // Ripiego: il dato reale vince dove c'e', la simulazione copre il resto.
    const auto covered = router.poll(now, 1s);
    require(covered.attitude.roll_deg.value == 7.5,
            "with the simulation in fallback the real attitude must win");
    require(!covered.attitude.roll_deg.is_simulated(),
            "a real attitude must not be reported as simulated");
    require(covered.position.latitude_deg.is_simulated(),
            "what the real source does not publish must be covered by the simulation");
    require(covered.position.latitude_deg.usable(),
            "the covered position must be usable at the bench");

    // Precedenza: serve a riprodurre uno scenario deterministico avendo comunque
    // i sensori collegati. Il dato reale continua ad arrivare, ma non e' quello
    // mostrato, e ogni campione lo dichiara.
    router.set_simulation_role(efis::SimulationRole::primary);
    const auto forced = router.poll(now + 20ms, 1020ms);
    require(forced.attitude.roll_deg.is_simulated(),
            "with the simulation as primary even the attitude must be simulated");
    require(forced.attitude.roll_deg.value != 7.5,
            "the simulated attitude must replace the real one, not echo it");

    // Disabilitata: cio' che i sensori non forniscono resta indisponibile. E'
    // l'unica configurazione onesta per una prova che debba dire qualcosa sul
    // comportamento reale.
    router.set_simulation_role(efis::SimulationRole::disabled);
    const auto bare = router.poll(now + 3s, 4s);
    require(bare.attitude.roll_deg.value == 7.5,
            "with the simulation disabled the real attitude must be shown");
    require(!bare.position.latitude_deg.usable(),
            "with the simulation disabled an absent position must stay absent");
    require(!bare.position.latitude_deg.is_simulated(),
            "nothing simulated may reach the snapshot when the role is disabled");

    // Lo scenario simulato avanza anche mentre e' disabilitato. Riattivandolo
    // non riparte da dove era stato spento: un salto, al banco, si scambia per
    // un guasto.
    const auto while_disabled = router.simulated_contribution();
    require(while_disabled.attitude.roll_deg.origin.signal == efis::SignalId::roll,
            "the simulation must keep being polled while disabled");
    router.set_simulation_role(efis::SimulationRole::fallback);
    const auto resumed = router.poll(now + 3s + 20ms, 4020ms);
    require(resumed.position.latitude_deg.is_simulated(),
            "re-enabling the simulation must cover the missing position again");

    // Guasto iniettato sul dato simulato. Rende la grandezza indisponibile, non
    // la porta a zero: uno zero e' un valore, e un valore si mostra.
    efis::FlightSourceRouter bench{{.reversion_dwell = 2000ms}};
    auto only_sim = std::make_shared<efis::SimulationSource>(42);
    bench.set_simulation(only_sim);
    const auto healthy = bench.poll(now, 1s);
    require(healthy.air_data.indicated_airspeed_kt.usable(),
            "without an injected fault the simulated airspeed must be usable");

    only_sim->set_fault(efis::InjectedFault::airspeed);
    const auto broken = bench.poll(now + 20ms, 1020ms);
    require(!broken.air_data.indicated_airspeed_kt.usable(),
            "an injected fault must make the quantity unavailable");
    require(broken.air_data.indicated_airspeed_kt.value != 0.0 ||
                broken.air_data.indicated_airspeed_kt.validity ==
                    efis::Validity::unavailable,
            "an unavailable quantity must not be presented as a plausible zero");

    require(std::string{efis::injected_fault_name(efis::InjectedFault::airspeed)} ==
                "AIRSPEED",
            "fault names must stay stable, they end up in the logs");
    require(std::string{efis::simulation_role_name(efis::SimulationRole::primary)} ==
                "PRIMARY",
            "role names must stay stable, they end up in the logs");

    // Un guasto sul simulato viene coperto dal dato reale, se c'e'. E' il caso
    // che rende utile l'iniezione: si prova la degradazione senza scollegare
    // nulla e senza restare al buio.
    efis::FlightSourceRouter mixed{{.reversion_dwell = 2000ms}};
    auto attitude_source = std::make_shared<AttitudeOnlySource>(3.25);
    auto covering_sim = std::make_shared<efis::SimulationSource>(42);
    mixed.add_source(attitude_source);
    mixed.set_simulation(covering_sim);
    const auto before_fault = mixed.poll(now, 1s);
    require(before_fault.attitude.roll_deg.value == 3.25,
            "the real attitude must be in use before the fault");
    require(mixed.source_count() == 1, "one real source must be registered");

    // --- Arbitrato del motore -------------------------------------------------
    //
    // Il motore ha diciotto grandezze su canali fisicamente distinti, quindi il
    // guasto parziale qui e' la norma. Un nodo sensori con due soli canali
    // collegati e' la condizione tipica del banco, non un caso limite.
    auto node = std::make_shared<PartialEngineNode>();
    auto engine_sim = std::make_shared<efis::EngineSimulationSource>(84);

    efis::EngineSourceRouter engine_router{{.reversion_dwell = 2000ms}};
    engine_router.add_source(node);
    engine_router.set_simulation(engine_sim);

    const auto engine_state = engine_router.poll(now, 1s);
    require(engine_state.rpm.value == 2500.0,
            "the real sensor node must win on the channel it actually measures");
    require(!engine_state.rpm.is_simulated(),
            "a measured rpm must not be reported as simulated");
    require(engine_state.cht_c[0].value == 191.0,
            "the first cylinder head must come from the node that measures it");
    require(engine_state.cht_c[1].is_simulated(),
            "a cylinder without a probe must be covered by the simulation");
    require(engine_state.oil_pressure_psi.is_simulated(),
            "an uncovered channel must fall back, not go blank");
    require(engine_router.arbitrator().selected_for(efis::SignalId::engine_rpm) ==
                efis::SourceId::esp32_legacy,
            "the arbitrator must report which source serves the rpm");
    require(engine_router.arbitrator().selected_for(efis::SignalId::oil_pressure) ==
                efis::SourceId::simulation,
            "and which source serves a channel nobody measures");

    // Un canale che cade non trascina gli altri: e' il motivo per cui si arbitra
    // per grandezza e non per sorgente.
    node->lose_rpm();
    const auto degraded = engine_router.poll(now + 100ms, 1100ms);
    require(degraded.rpm.is_simulated(),
            "losing the rpm channel must fall back on that channel");
    require(degraded.cht_c[0].value == 191.0,
            "the cylinder head that still measures must keep its real value");
    require(!degraded.cht_c[0].is_simulated(),
            "a channel that never failed must not become simulated");
    require(engine_router.arbitrator().statistics().failovers >= 1,
            "the failover must be counted for the black box");

    // Con la simulazione disabilitata restano soltanto i canali realmente
    // misurati. Il quadro motore non puo' piu' mostrare numeri dove non c'e'
    // misura: era esattamente il difetto che si vedeva sul display.
    engine_router.set_simulation_role(efis::SimulationRole::disabled);
    const auto bare_engine = engine_router.poll(now + 3s, 4s);
    require(bare_engine.cht_c[0].usable(),
            "a measured cylinder head stays usable without the simulation");
    require(!bare_engine.oil_pressure_psi.usable(),
            "an unmeasured channel must not be usable when nothing simulates it");
    require(!bare_engine.fuel_left_gal.usable(),
            "fuel quantity nobody measures must not show a plausible number");

    // E soprattutto: senza misure la salute non puo' dire "normale". E' il dato
    // falso piu' pericoloso, perche' rassicura invece di allarmare.
    efis::EngineSourceRouter silent{{.reversion_dwell = 2000ms}};
    const auto nothing_at_all = silent.poll(now, 1s);
    require(nothing_at_all.health == efis::engine::EngineHealth::warning,
            "an engine with no data at all must not be reported as healthy");
    require(!nothing_at_all.rpm.usable(),
            "with no source registered no channel can be usable");

    // Il guasto iniettato resta nella sorgente simulata e non altera i canali
    // veri.
    efis::EngineSourceRouter bench_engine{{.reversion_dwell = 2000ms}};
    auto faulted_sim = std::make_shared<efis::EngineSimulationSource>(84);
    bench_engine.set_simulation(faulted_sim);
    faulted_sim->set_fault(efis::engine::EngineFault::low_oil_pressure);
    const auto with_fault = bench_engine.poll(now, 1s);
    require(with_fault.oil_pressure_psi.value == 22.0,
            "the injected fault must reach the arbitrated engine state");
    require(with_fault.injected_fault == efis::engine::EngineFault::low_oil_pressure,
            "the arbitrated state must declare which fault was injected");

    std::cout << "All EFIS arbitration tests passed\n";
}
