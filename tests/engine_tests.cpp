// Prove della sezione motore.
//
// Hanno un eseguibile proprio perche' il nodo EMS e' un processo proprio: cio'
// che verificano qui e' il contratto che quel nodo pubblica, non il calcolo del
// nucleo. La proprieta' che conta di piu' non e' il valore: e' che ogni
// grandezza continui a dichiararsi simulata anche mentre finge di essere
// guasta, perche' il giorno in cui arriveranno i sensori veri i due casi
// dovranno restare distinguibili senza leggere il codice.

#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/core/engine_runtime.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// Tutti i canali dello snapshot, nell'ordine in cui il modello li dichiara.
// Serve a verificare le proprieta' che valgono per ogni grandezza senza doverle
// ripetere diciotto volte. Una grandezza aggiunta al modello e dimenticata qui
// e' l'unico modo per cui queste prove possono diventare bugiarde, quindi il
// conteggio atteso e' verificato esplicitamente.
std::vector<const efis::Sample<double>*> channels(
    const efis::engine::EngineSnapshot& snapshot) {
    std::vector<const efis::Sample<double>*> out{
        &snapshot.rpm, &snapshot.manifold_pressure_inhg,
        &snapshot.oil_pressure_psi, &snapshot.oil_temperature_c,
        &snapshot.fuel_pressure_psi, &snapshot.fuel_flow_gph,
        &snapshot.fuel_left_gal, &snapshot.fuel_right_gal,
        &snapshot.bus_voltage_v, &snapshot.alternator_current_a};
    for (const auto& sample : snapshot.cht_c) out.push_back(&sample);
    for (const auto& sample : snapshot.egt_c) out.push_back(&sample);
    return out;
}

constexpr efis::engine::EngineFault all_faults[] = {
    efis::engine::EngineFault::low_oil_pressure,
    efis::engine::EngineFault::high_cht,
    efis::engine::EngineFault::alternator_failure,
    efis::engine::EngineFault::low_fuel_pressure};

void provenance_survives_every_fault() {
    const auto epoch = efis::MonoClock::now();
    std::vector<efis::engine::EngineFault> cases{efis::engine::EngineFault::none};
    for (const auto fault : all_faults) cases.push_back(fault);

    for (const auto fault : cases) {
        efis::EngineSimulationProvider provider{84};
        provider.set_fault(fault);
        const auto snapshot = provider.sample(epoch + 1s, 1s);
        const auto samples = channels(snapshot);
        require(samples.size() == 18,
                "engine snapshot must expose eighteen channels");
        for (const auto* sample : samples) {
            require(sample->source == efis::SourceId::simulation,
                    "every engine channel must name the simulation as its source");
            require(sample->validity == efis::Validity::simulated,
                    "every engine channel must stay explicitly simulated");
            require(sample->origin.simulated,
                    "the simulated marker must survive fault injection");
            require(sample->is_simulated(),
                    "no engine channel may ever present itself as real data");
            require(sample->origin.signal != efis::SignalId::unknown,
                    "every engine channel must declare which signal it carries");
            require(sample->origin.unit != efis::Unit::unknown,
                    "every engine channel must declare its unit");
        }
    }
}

void every_fault_changes_the_engine_and_its_health() {
    const auto epoch = efis::MonoClock::now();
    efis::EngineSimulationProvider nominal_provider{84};
    const auto nominal = nominal_provider.sample(epoch + 1s, 1s);
    require(nominal.health == efis::engine::EngineHealth::normal,
            "an engine without injected faults must report normal health");

    for (const auto fault : all_faults) {
        efis::EngineSimulationProvider provider{84};
        provider.set_fault(fault);
        const auto faulted = provider.sample(epoch + 1s, 1s);
        require(faulted.injected_fault == fault,
                "the snapshot must report which fault produced it");
        require(faulted.health != efis::engine::EngineHealth::normal,
                "an injected fault must leave the engine health above normal");

        const auto before = channels(nominal);
        const auto after = channels(faulted);
        std::size_t altered = 0;
        for (std::size_t index = 0; index < after.size(); ++index) {
            if (after[index]->value == before[index]->value) continue;
            ++altered;
            // Un guasto altera la grandezza misurata, quindi anche il valore
            // grezzo. Un raw rimasto nominale descriverebbe un sensore sano e
            // una conversione sbagliata, che e' un guasto diverso da quello
            // iniettato: e' il dato plausibile ma falso della ADR-004.
            require(after[index]->raw == after[index]->value,
                    "an injected fault must alter the raw value as well");
        }
        require(altered > 0, "an injected fault must alter at least one channel");
    }
}

void cylinders_are_four_and_distinctly_identified() {
    const auto epoch = efis::MonoClock::now();
    efis::EngineSimulationProvider provider{7};
    const auto snapshot = provider.sample(epoch + 1s, 1s);
    require(snapshot.cht_c.size() == 4 && snapshot.egt_c.size() == 4,
            "the engine must publish four cylinders on both CHT and EGT");
    for (std::size_t index = 0; index < snapshot.cht_c.size(); ++index) {
        const auto offset = static_cast<std::uint16_t>(index);
        const auto expected_cht = static_cast<efis::SignalId>(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(efis::SignalId::cylinder_head_temperature_1) + offset));
        const auto expected_egt = static_cast<efis::SignalId>(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(efis::SignalId::exhaust_gas_temperature_1) + offset));
        require(snapshot.cht_c[index].origin.signal == expected_cht,
                "each CHT channel must carry its own cylinder signal");
        require(snapshot.egt_c[index].origin.signal == expected_egt,
                "each EGT channel must carry its own cylinder signal");
    }
}

void sequence_advances_once_per_sample() {
    const auto epoch = efis::MonoClock::now();
    efis::EngineSimulationProvider provider{84};
    std::uint64_t previous = 0;
    for (int step = 1; step <= 5; ++step) {
        const auto snapshot = provider.sample(epoch + std::chrono::seconds{step},
                                              std::chrono::seconds{step});
        require(snapshot.sequence == previous + 1,
                "the engine sequence must advance by one on every sample");
        previous = snapshot.sequence;
    }
}

void the_same_elapsed_time_yields_the_same_engine() {
    const auto epoch = efis::MonoClock::now();
    efis::EngineSimulationProvider first{84};
    efis::EngineSimulationProvider second{84};
    const auto a = first.sample(epoch + 3s, 3s);
    const auto b = second.sample(epoch + 3s, 3s);
    const auto left = channels(a);
    const auto right = channels(b);
    for (std::size_t index = 0; index < left.size(); ++index) {
        require(left[index]->value == right[index]->value,
                "the engine simulation must replay identically for a given elapsed time");
    }
}

void runtime_counters_keep_distinct_meanings() {
    efis::engine::RuntimeTracker tracker{{500.0, 5000.0}, {10.0, 8.0, 7.0, 6.0}};
    tracker.update(0.0, 3600.0, false);
    require(!tracker.engine_running(), "master power alone must not count as engine time");
    require(tracker.totals().master_hours == 11.0 && tracker.totals().engine_hours == 8.0,
            "master and engine hours must remain independent");

    tracker.update(2500.0, 3600.0, true);
    require(tracker.engine_running(), "rpm above the configured threshold must count as running");
    require(tracker.totals().engine_hours == 9.0,
            "one running hour must add one engine hour");
    require(tracker.totals().tach_hours == 7.5,
            "tach time must be weighted by the configured reference rpm");
    require(tracker.totals().flight_hours == 7.0,
            "airborne time must be tracked separately from engine time");
}

}  // namespace

int main() {
    provenance_survives_every_fault();
    every_fault_changes_the_engine_and_its_health();
    cylinders_are_four_and_distinctly_identified();
    sequence_advances_once_per_sample();
    the_same_elapsed_time_yields_the_same_engine();
    runtime_counters_keep_distinct_meanings();
    std::cout << "engine tests passed\n";
    return 0;
}
