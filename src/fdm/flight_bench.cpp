#include "efis/fdm/flight_bench.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace efis::fdm {
namespace {

constexpr double litres_per_gallon = 3.785411784;
constexpr double avgas_density_kg_l = 0.72;
constexpr double jet_fuel_density_kg_l = 0.80;

// Fabbrica del campione simulato.
//
// Ripete deliberatamente la forma usata dai provider esistenti: stessa
// validita', stesso marcatore, stesso riempimento del valore grezzo. La
// ripetizione e' preferibile a una funzione condivisa in un'intestazione di
// comodo, perche' questo motore vive accanto a quei provider e non al loro
// posto, e un giorno uno dei due potra' cambiare senza trascinare l'altro.
[[nodiscard]] Sample<double> simulated(double value, SignalId signal, Unit unit,
                                       MonoTime now, std::uint64_t sequence) {
    Sample<double> sample{value, SourceId::simulation, now, now, Quality::high,
                          Validity::simulated, sequence};
    sample.origin.signal = signal;
    sample.origin.unit = unit;
    sample.origin.simulated = true;
    sample.raw = value;
    return sample;
}

// Campione dichiarato non disponibile.
//
// Serve al satellitare perso: il valore resta quello dell'ultimo fix e la
// validita' dice che non vale. Mettere uno zero sarebbe peggio del guasto,
// perche' zero e zero sono le coordinate di un punto nel golfo di Guinea.
[[nodiscard]] Sample<double> unavailable(double last_value, SignalId signal, Unit unit,
                                         MonoTime now, std::uint64_t sequence) {
    Sample<double> sample{last_value, SourceId::simulation, now, now, Quality::unknown,
                          Validity::unavailable, sequence};
    sample.origin.signal = signal;
    sample.origin.unit = unit;
    sample.origin.simulated = true;
    sample.raw = last_value;
    return sample;
}

[[nodiscard]] SignalId cylinder_signal(SignalId first, std::size_t index) {
    return static_cast<SignalId>(
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(first) +
                                   static_cast<std::uint16_t>(index)));
}

// Traduzione del guasto del propulsore nel vocabolario della sezione motore
// esistente. Non e' biunivoca, e non va fatta sembrare tale: il cilindro muto e
// lo spegnimento in volo non hanno un corrispondente in EngineFault, e
// dichiarare `none` e' corretto perche' quell'enumerazione descrive cio' che il
// quadro motore sa rappresentare, non cio' che il motore puo' subire.
[[nodiscard]] engine::EngineFault map_fault(PropulsionFault fault) noexcept {
    switch (fault) {
        case PropulsionFault::oil_loss: return engine::EngineFault::low_oil_pressure;
        case PropulsionFault::alternator: return engine::EngineFault::alternator_failure;
        case PropulsionFault::fuel_pump: return engine::EngineFault::low_fuel_pressure;
        case PropulsionFault::single_cylinder:
        case PropulsionFault::flameout:
        case PropulsionFault::none: return engine::EngineFault::none;
    }
    return engine::EngineFault::none;
}

[[nodiscard]] engine::EngineHealth map_health(PropulsionFault fault) noexcept {
    switch (fault) {
        case PropulsionFault::oil_loss:
        case PropulsionFault::fuel_pump:
        case PropulsionFault::flameout: return engine::EngineHealth::warning;
        case PropulsionFault::alternator:
        case PropulsionFault::single_cylinder: return engine::EngineHealth::caution;
        case PropulsionFault::none: return engine::EngineHealth::normal;
    }
    return engine::EngineHealth::normal;
}

}  // namespace

FlightBench::FlightBench(Airframe airframe, Environment environment,
                         SensorPolicy sensor_policy, std::uint64_t seed)
    : vehicle_(std::move(airframe), environment),
      sensors_(sensor_policy, seed) {
    sensors_.set_magnetic_variation_deg(environment.magnetic_variation_deg);
    align_sensors();
}

void FlightBench::align_sensors() {
    const AirState air = vehicle_.environment().atmosphere.at_altitude(
        vehicle_.state().altitude_m);
    sensors_.set_magnetic_variation_deg(vehicle_.environment().magnetic_variation_deg);
    sensors_.align(vehicle_.state(), air);
}

void FlightBench::step_seconds(double seconds) {
    if (seconds <= 0.0) {
        return;
    }
    vehicle_.step(seconds);
    const AirState air = vehicle_.environment().atmosphere.at_altitude(
        vehicle_.state().altitude_m);
    sensors_.update(vehicle_.state(), air, seconds);
    ++sequence_;
}

void FlightBench::step(std::chrono::milliseconds step) {
    step_seconds(static_cast<double>(step.count()) / 1000.0);
}

FlightSnapshot FlightBench::flight_snapshot(MonoTime now) const {
    const IndicatedState& indicated = sensors_.indicated();

    FlightSnapshot out{};
    out.sequence = sequence_;
    out.timestamp = now;

    out.attitude.roll_deg = simulated(indicated.roll_deg, SignalId::roll, Unit::degree,
                                      now, sequence_);
    out.attitude.pitch_deg = simulated(indicated.pitch_deg, SignalId::pitch,
                                       Unit::degree, now, sequence_);
    out.attitude.heading_deg = simulated(indicated.magnetic_heading_deg,
                                         SignalId::heading, Unit::degree, now,
                                         sequence_);
    out.attitude.turn_rate_deg_s = simulated(indicated.turn_rate_deg_s,
                                             SignalId::turn_rate,
                                             Unit::degree_per_second, now, sequence_);
    out.attitude.lateral_acceleration_g = simulated(indicated.lateral_acceleration_g,
                                                    SignalId::lateral_acceleration,
                                                    Unit::gravity, now, sequence_);

    out.air_data.indicated_airspeed_kt = simulated(indicated.indicated_airspeed_kt,
                                                   SignalId::indicated_airspeed,
                                                   Unit::knot, now, sequence_);
    out.air_data.pressure_altitude_ft = simulated(indicated.pressure_altitude_ft,
                                                  SignalId::pressure_altitude,
                                                  Unit::foot, now, sequence_);
    out.air_data.vertical_speed_fpm = simulated(indicated.vertical_speed_fpm,
                                                SignalId::vertical_speed,
                                                Unit::foot_per_minute, now, sequence_);
    out.air_data.indicated_altitude_ft = simulated(indicated.indicated_altitude_ft,
                                                   SignalId::indicated_altitude,
                                                   Unit::foot, now, sequence_);

    // La regolazione barometrica non e' una misura: la provenienza lo dice, e
    // il modello dati la tiene distinta proprio perche' rileggendo un volo si
    // deve poter sapere se la regolazione era scelta o ereditata.
    out.air_data.baro_setting_hpa = simulated(indicated.baro_setting_hpa,
                                              SignalId::baro_setting, Unit::hectopascal,
                                              now, sequence_);
    out.air_data.baro_setting_hpa.source = SourceId::pilot_setting;

    if (indicated.gnss_valid) {
        out.position.latitude_deg = simulated(indicated.latitude_deg, SignalId::latitude,
                                              Unit::degree, now, sequence_);
        out.position.longitude_deg = simulated(indicated.longitude_deg,
                                               SignalId::longitude, Unit::degree, now,
                                               sequence_);
        out.position.ground_speed_kt = simulated(indicated.ground_speed_kt,
                                                 SignalId::ground_speed, Unit::knot,
                                                 now, sequence_);
        out.position.gnss_vertical_speed_fpm = simulated(indicated.gnss_vertical_speed_fpm,
                                                         SignalId::gnss_vertical_speed,
                                                         Unit::foot_per_minute, now,
                                                         sequence_);

        // Qualita' dedotta dal numero di satelliti e dalla dispersione: e' il
        // giudizio che il ricevitore esprime su se stesso, e passarlo oltre
        // permette al display di distinguere un dato buono da uno accettato per
        // mancanza d'altro.
        const Quality quality = indicated.satellites >= 8 && indicated.horizontal_dilution < 2.0
                                    ? Quality::high
                                    : Quality::poor;
        out.position.latitude_deg.quality = quality;
        out.position.longitude_deg.quality = quality;
        out.position.ground_speed_kt.quality = quality;
        out.position.gnss_vertical_speed_fpm.quality = quality;
    } else {
        out.position.latitude_deg = unavailable(indicated.latitude_deg,
                                                SignalId::latitude, Unit::degree, now,
                                                sequence_);
        out.position.longitude_deg = unavailable(indicated.longitude_deg,
                                                 SignalId::longitude, Unit::degree, now,
                                                 sequence_);
        out.position.ground_speed_kt = unavailable(indicated.ground_speed_kt,
                                                   SignalId::ground_speed, Unit::knot,
                                                   now, sequence_);
        out.position.gnss_vertical_speed_fpm =
            unavailable(indicated.gnss_vertical_speed_fpm, SignalId::gnss_vertical_speed,
                        Unit::foot_per_minute, now, sequence_);
    }

    // Guida e navigazione restano vuote. Il motore di volo non ha un pilota
    // automatico ne' una radio: dichiararle indisponibili e' l'unica risposta
    // vera, e riempirle con valori inventati sarebbe il dato plausibile ma
    // falso prodotto da noi, che ADR-004 vieta esplicitamente.
    return out;
}

engine::EngineSnapshot FlightBench::engine_snapshot(MonoTime now) const {
    const PropulsionState& engine_state = vehicle_.engine();
    const PropulsionFault fault = vehicle_.propulsion().fault();
    const bool piston = vehicle_.airframe().propulsion.kind == EngineKind::piston_prop;
    const double density = piston ? avgas_density_kg_l : jet_fuel_density_kg_l;

    engine::EngineSnapshot out{};
    out.sequence = sequence_;
    out.timestamp = now;
    out.injected_fault = map_fault(fault);
    out.health = map_health(fault);

    out.rpm = simulated(engine_state.engine_rpm, SignalId::engine_rpm,
                        Unit::revolutions_per_minute, now, sequence_);
    out.manifold_pressure_inhg = simulated(engine_state.manifold_pressure_inhg,
                                           SignalId::manifold_pressure,
                                           Unit::inch_mercury, now, sequence_);
    out.oil_pressure_psi = simulated(engine_state.oil_pressure_psi,
                                     SignalId::oil_pressure,
                                     Unit::pound_per_square_inch, now, sequence_);
    out.oil_temperature_c = simulated(engine_state.oil_temperature_c,
                                      SignalId::oil_temperature, Unit::celsius, now,
                                      sequence_);
    out.fuel_pressure_psi = simulated(engine_state.fuel_pressure_psi,
                                      SignalId::fuel_pressure,
                                      Unit::pound_per_square_inch, now, sequence_);

    const double fuel_flow_gph = engine_state.fuel_flow_kg_s * 3600.0 / density /
                                 litres_per_gallon;
    out.fuel_flow_gph = simulated(fuel_flow_gph, SignalId::fuel_flow,
                                  Unit::gallon_per_hour, now, sequence_);

    // Due serbatoi uguali: il modello ha una massa di carburante sola e non
    // sposta nulla fra le ali. Dichiararlo conta, perche' una prova sullo
    // sbilanciamento fra serbatoi qui non si puo' fare, e non deve sembrare
    // che si possa.
    const double half_gallons = vehicle_.state().fuel_kg / density / litres_per_gallon *
                                0.5;
    out.fuel_left_gal = simulated(half_gallons, SignalId::fuel_quantity_left,
                                  Unit::gallon, now, sequence_);
    out.fuel_right_gal = simulated(half_gallons, SignalId::fuel_quantity_right,
                                   Unit::gallon, now, sequence_);

    out.bus_voltage_v = simulated(engine_state.bus_voltage_v, SignalId::bus_voltage,
                                  Unit::volt, now, sequence_);
    out.alternator_current_a = simulated(engine_state.alternator_current_a,
                                         SignalId::alternator_current, Unit::ampere, now,
                                         sequence_);

    for (std::size_t index = 0; index < out.cht_c.size(); ++index) {
        if (piston) {
            out.cht_c[index] = simulated(engine_state.cylinder_head_c[index],
                                         cylinder_signal(SignalId::cylinder_head_temperature_1,
                                                         index),
                                         Unit::celsius, now, sequence_);
            out.egt_c[index] = simulated(engine_state.exhaust_gas_c[index],
                                         cylinder_signal(SignalId::exhaust_gas_temperature_1,
                                                         index),
                                         Unit::celsius, now, sequence_);
        } else {
            // Una turbina non ha teste ne' quattro scarichi. I canali restano
            // indisponibili: la temperatura fra turbine viaggia sul bus delle
            // variabili, che sa rappresentarla, e non in un campo che
            // significa un'altra cosa.
            out.cht_c[index] = unavailable(0.0,
                                           cylinder_signal(SignalId::cylinder_head_temperature_1,
                                                           index),
                                           Unit::celsius, now, sequence_);
            out.egt_c[index] = unavailable(0.0,
                                           cylinder_signal(SignalId::exhaust_gas_temperature_1,
                                                           index),
                                           Unit::celsius, now, sequence_);
        }
    }
    return out;
}

void FlightBench::publish_bus(std::vector<SimVarSample>& out) const {
    const BusInputs inputs{vehicle_.state(), sensors_.indicated(), vehicle_.engine(),
                           vehicle_.airframe(), vehicle_.environment(),
                           vehicle_.controls()};
    publish_sim_vars(inputs, out);
}

FlightSnapshot BenchFlightSource::poll(MonoTime now,
                                       std::chrono::milliseconds elapsed) {
    (void)elapsed;
    return bench_->flight_snapshot(now);
}

engine::EngineSnapshot BenchEngineSource::poll(MonoTime now,
                                               std::chrono::milliseconds elapsed) {
    (void)elapsed;
    return bench_->engine_snapshot(now);
}

}  // namespace efis::fdm
