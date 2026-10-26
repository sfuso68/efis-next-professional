#pragma once

#include "efis/core/engine_source.hpp"
#include "efis/core/flight_source.hpp"
#include "efis/engine/engine_snapshot.hpp"
#include "efis/fdm/dynamics.hpp"
#include "efis/fdm/sensors.hpp"
#include "efis/fdm/sim_vars.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

namespace efis::fdm {

// Il banco: dinamica, sensori e pubblicazione verso l'EFIS esistente.
//
// Tiene insieme le tre parti e le espone con le interfacce che il display
// consuma gia', FlightSource e EngineSource. E' qui che il motore di volo
// diventa utilizzabile dal resto del programma senza che il resto del programma
// sappia della sua esistenza: ADR-003 vuole che il display non conosca la
// provenienza dei dati, e un motore di volo che si presentasse come un caso
// speciale violerebbe quella regola proprio nel punto in cui conta, cioe'
// quando bisogna confrontarlo con un sensore vero collegato nello stesso
// istante.
//
// Ogni campione prodotto porta Validity::simulated e origin.simulated. Non e'
// una formalita': e' il vincolo del paragrafo 19 dell'handoff, e questo motore
// lo rende piu' importante di prima, non meno, perche' i suoi numeri sono
// plausibili in modo che un copione non riesce a essere.

class FlightBench final {
public:
    FlightBench(Airframe airframe, Environment environment,
                SensorPolicy sensor_policy = {}, std::uint64_t seed = 20261026);

    [[nodiscard]] Vehicle& vehicle() noexcept { return vehicle_; }
    [[nodiscard]] const Vehicle& vehicle() const noexcept { return vehicle_; }
    [[nodiscard]] SensorSuite& sensors() noexcept { return sensors_; }
    [[nodiscard]] const SensorSuite& sensors() const noexcept { return sensors_; }

    // Riallinea i sensori dopo un riposizionamento. Da chiamare dopo ogni
    // place_on_ground o place_in_flight, altrimenti le linee pneumatiche
    // inseguono per secondi una quota che l'aeroplano non ha mai avuto.
    void align_sensors();

    void step(std::chrono::milliseconds step);
    void step_seconds(double seconds);

    [[nodiscard]] FlightSnapshot flight_snapshot(MonoTime now) const;
    [[nodiscard]] engine::EngineSnapshot engine_snapshot(MonoTime now) const;
    void publish_bus(std::vector<SimVarSample>& out) const;

    [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }

private:
    Vehicle vehicle_;
    SensorSuite sensors_;
    std::uint64_t sequence_{};
};

// Il banco visto come sorgente di volo. Condivide il banco invece di
// possederlo: la stessa esecuzione alimenta volo e motore, e due copie
// darebbero due aeroplani diversi sullo stesso display.
class BenchFlightSource final : public FlightSource {
public:
    explicit BenchFlightSource(std::shared_ptr<FlightBench> bench) noexcept
        : bench_(std::move(bench)) {}

    [[nodiscard]] SourceId source() const noexcept override {
        return SourceId::simulation;
    }

    // Legge l'ultimo stato pubblicato dal banco e non lo fa avanzare.
    //
    // Avanzare qui sarebbe sbagliato in un modo che si vedrebbe tardi: due
    // sorgenti registrate sullo stesso banco, volo e motore, lo farebbero
    // avanzare due volte per fotogramma, e l'aeroplano volerebbe al doppio
    // della velocita' dichiarata. Il tempo appartiene a chi possiede il
    // ciclo, e il ciclo chiama step().
    [[nodiscard]] FlightSnapshot poll(MonoTime now,
                                      std::chrono::milliseconds elapsed) override;

private:
    std::shared_ptr<FlightBench> bench_;
};

class BenchEngineSource final : public EngineSource {
public:
    explicit BenchEngineSource(std::shared_ptr<FlightBench> bench) noexcept
        : bench_(std::move(bench)) {}

    [[nodiscard]] SourceId source() const noexcept override {
        return SourceId::simulation;
    }

    [[nodiscard]] engine::EngineSnapshot poll(
        MonoTime now, std::chrono::milliseconds elapsed) override;

private:
    std::shared_ptr<FlightBench> bench_;
};

}  // namespace efis::fdm
