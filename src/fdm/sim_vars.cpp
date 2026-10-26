#include "efis/fdm/sim_vars.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace efis::fdm {
namespace {

constexpr double radians_to_degrees = 180.0 / std::numbers::pi;
constexpr double litres_per_gallon = 3.785411784;

// Densita' della benzina avio e del jet: servono a passare dalla massa, che e'
// cio' che il modello consuma, al volume, che e' cio' che lo strumento mostra.
// Sono due numeri diversi e vanno tenuti distinti, perche' un litro di jet pesa
// il dieci per cento in piu' e su un rifornimento completo la differenza e'
// decine di chili.
constexpr double avgas_density_kg_l = 0.72;
constexpr double jet_fuel_density_kg_l = 0.80;

void add(std::vector<SimVarSample>& out, std::string_view name, std::string_view unit,
         double value, NameOrigin origin, bool available = true) {
    out.push_back(SimVarSample{name, unit, value, available, origin});
}

}  // namespace

const char* name_origin_label(NameOrigin origin) noexcept {
    switch (origin) {
        case NameOrigin::confirmed_in_sdk: return "SDK";
        case NameOrigin::documented: return "DOC";
        case NameOrigin::bench_extension: return "BENCH";
    }
    return "DOC";
}

void publish_sim_vars(const BusInputs& inputs, std::vector<SimVarSample>& out) {
    out.clear();
    out.reserve(72);

    const VehicleState& truth = inputs.truth;
    const IndicatedState& indicated = inputs.indicated;
    const PropulsionState& engine = inputs.engine;

    // --- Dati aria ---------------------------------------------------------
    //
    // Valori misurati, non veri: e' la differenza deliberata rispetto a Flight
    // Simulator, spiegata nell'intestazione. Qui sta il senso del banco.
    add(out, "AIRSPEED INDICATED", "knots", indicated.indicated_airspeed_kt,
        NameOrigin::confirmed_in_sdk, indicated.air_data_valid);
    add(out, "AIRSPEED TRUE", "knots",
        truth.true_airspeed_m_s * knots_per_metre_per_second,
        NameOrigin::confirmed_in_sdk);
    add(out, "AIRSPEED MACH", "mach", indicated.mach, NameOrigin::documented);
    add(out, "INDICATED ALTITUDE", "feet", indicated.indicated_altitude_ft,
        NameOrigin::confirmed_in_sdk, indicated.air_data_valid);
    add(out, "PRESSURE ALTITUDE", "feet", indicated.pressure_altitude_ft,
        NameOrigin::documented, indicated.air_data_valid);
    add(out, "VERTICAL SPEED", "feet per minute", indicated.vertical_speed_fpm,
        NameOrigin::confirmed_in_sdk, indicated.air_data_valid);
    add(out, "KOHLSMAN SETTING MB", "millibars", indicated.baro_setting_hpa,
        NameOrigin::confirmed_in_sdk);
    add(out, "KOHLSMAN SETTING HG", "inHg",
        indicated.baro_setting_hpa * pascal_per_hectopascal / pascal_per_inch_mercury,
        NameOrigin::confirmed_in_sdk);
    add(out, "AMBIENT PRESSURE", "inHg",
        indicated.static_pressure_pa / pascal_per_inch_mercury,
        NameOrigin::confirmed_in_sdk);
    add(out, "AMBIENT TEMPERATURE", "celsius", truth.outside_air_temperature_c,
        NameOrigin::confirmed_in_sdk);
    add(out, "TOTAL AIR TEMPERATURE", "celsius", indicated.total_air_temperature_c,
        NameOrigin::documented);
    add(out, "AMBIENT WIND VELOCITY", "knots", inputs.environment.wind.speed_kt,
        NameOrigin::confirmed_in_sdk);
    add(out, "AMBIENT WIND DIRECTION", "degrees", inputs.environment.wind.from_deg,
        NameOrigin::confirmed_in_sdk);

    // --- Assetto -----------------------------------------------------------
    //
    // In Flight Simulator queste variabili si chiamano "DEGREES" e si leggono
    // in radianti, perche' il nome ricorda l'unita' del simulatore di
    // trent'anni fa e il valore no. Qui l'unita' dichiarata e' quella in cui
    // il valore e' espresso davvero: un bus che dichiara gradi e porta
    // radianti e' una trappola, e una trappola nota da trent'anni resta una
    // trappola.
    add(out, "PLANE PITCH DEGREES", "degrees", indicated.pitch_deg,
        NameOrigin::confirmed_in_sdk, indicated.attitude_valid);
    add(out, "PLANE BANK DEGREES", "degrees", indicated.roll_deg,
        NameOrigin::confirmed_in_sdk, indicated.attitude_valid);
    add(out, "PLANE HEADING DEGREES MAGNETIC", "degrees", indicated.magnetic_heading_deg,
        NameOrigin::confirmed_in_sdk, indicated.attitude_valid);
    add(out, "PLANE HEADING DEGREES TRUE", "degrees", indicated.true_heading_deg,
        NameOrigin::documented, indicated.attitude_valid);
    add(out, "TURN INDICATOR RATE", "degrees per second", indicated.turn_rate_deg_s,
        NameOrigin::confirmed_in_sdk, indicated.attitude_valid);
    add(out, "G FORCE", "gforce", indicated.normal_acceleration_g,
        NameOrigin::confirmed_in_sdk);
    add(out, "ACCELERATION BODY Y", "gforce", indicated.lateral_acceleration_g,
        NameOrigin::documented);
    add(out, "INCIDENCE ALPHA", "degrees", indicated.angle_of_attack_deg,
        NameOrigin::confirmed_in_sdk);
    add(out, "INCIDENCE BETA", "degrees", truth.sideslip_rad * radians_to_degrees,
        NameOrigin::documented);
    add(out, "STALL WARNING", "bool", truth.stalled ? 1.0 : 0.0, NameOrigin::documented);
    add(out, "SIM ON GROUND", "bool", truth.on_ground ? 1.0 : 0.0,
        NameOrigin::confirmed_in_sdk);

    // --- Posizione ---------------------------------------------------------
    add(out, "PLANE LATITUDE", "degrees", indicated.latitude_deg,
        NameOrigin::documented, indicated.gnss_valid);
    add(out, "PLANE LONGITUDE", "degrees", indicated.longitude_deg,
        NameOrigin::documented, indicated.gnss_valid);
    add(out, "PLANE ALTITUDE", "feet", indicated.gnss_altitude_ft,
        NameOrigin::documented, indicated.gnss_valid);
    add(out, "GPS GROUND SPEED", "knots", indicated.ground_speed_kt,
        NameOrigin::confirmed_in_sdk, indicated.gnss_valid);
    add(out, "GPS GROUND TRUE TRACK", "degrees", indicated.track_deg,
        NameOrigin::documented, indicated.gnss_valid);
    add(out, "GROUND VELOCITY", "knots", indicated.ground_speed_kt,
        NameOrigin::confirmed_in_sdk, indicated.gnss_valid);

    // --- Comandi -----------------------------------------------------------
    add(out, "GENERAL ENG THROTTLE LEVER POSITION:1", "percent",
        inputs.controls.throttle * 100.0, NameOrigin::documented);
    add(out, "ELEVATOR TRIM PCT", "percent", inputs.controls.elevator_trim * 100.0,
        NameOrigin::confirmed_in_sdk);
    add(out, "FLAPS HANDLE PERCENT", "percent",
        inputs.airframe.flaps.empty() || inputs.airframe.flaps.back().angle_deg <= 0.0
            ? 0.0
            : inputs.controls.flap_deg / inputs.airframe.flaps.back().angle_deg * 100.0,
        NameOrigin::confirmed_in_sdk);
    add(out, "GEAR HANDLE POSITION", "bool", inputs.controls.gear_down ? 1.0 : 0.0,
        NameOrigin::confirmed_in_sdk);

    // --- Motore ------------------------------------------------------------
    const bool piston = inputs.airframe.propulsion.kind == EngineKind::piston_prop;
    const double fuel_density = piston ? avgas_density_kg_l : jet_fuel_density_kg_l;

    add(out, "GENERAL ENG COMBUSTION:1", "bool", engine.running ? 1.0 : 0.0,
        NameOrigin::confirmed_in_sdk);
    add(out, "GENERAL ENG RPM:1", "rpm", engine.engine_rpm,
        NameOrigin::confirmed_in_sdk);
    add(out, "PROP RPM:1", "rpm", engine.propeller_rpm, NameOrigin::documented);
    add(out, "GENERAL ENG MANIFOLD PRESSURE:1", "inHg", engine.manifold_pressure_inhg,
        NameOrigin::documented, piston);
    add(out, "GENERAL ENG OIL PRESSURE:1", "psi", engine.oil_pressure_psi,
        NameOrigin::documented);
    add(out, "GENERAL ENG OIL TEMPERATURE:1", "celsius", engine.oil_temperature_c,
        NameOrigin::documented);
    add(out, "GENERAL ENG FUEL PRESSURE:1", "psi", engine.fuel_pressure_psi,
        NameOrigin::documented);
    add(out, "ENG FUEL FLOW GPH:1", "gallons per hour",
        engine.fuel_flow_kg_s * 3600.0 / fuel_density / litres_per_gallon,
        NameOrigin::documented);

    for (std::size_t index = 0; index < engine.cylinder_head_c.size(); ++index) {
        // I nomi indicizzati del bus devono esistere come stringhe stabili: un
        // nome composto a ogni fotogramma sarebbe un'allocazione per variabile
        // e per fotogramma, e il campione porta una vista e non una copia.
        static constexpr std::string_view head_names[] = {
            "RECIP ENG CYLINDER HEAD TEMPERATURE:1",
            "RECIP ENG CYLINDER HEAD TEMPERATURE:2",
            "RECIP ENG CYLINDER HEAD TEMPERATURE:3",
            "RECIP ENG CYLINDER HEAD TEMPERATURE:4"};
        static constexpr std::string_view exhaust_names[] = {
            "ENG EXHAUST GAS TEMPERATURE:1", "ENG EXHAUST GAS TEMPERATURE:2",
            "ENG EXHAUST GAS TEMPERATURE:3", "ENG EXHAUST GAS TEMPERATURE:4"};
        add(out, head_names[index], "celsius", engine.cylinder_head_c[index],
            NameOrigin::confirmed_in_sdk, piston);
        add(out, exhaust_names[index], "celsius", engine.exhaust_gas_c[index],
            NameOrigin::confirmed_in_sdk, piston);
    }

    add(out, "TURB ENG N1:1", "percent", engine.gas_generator_pct,
        NameOrigin::confirmed_in_sdk, !piston);
    add(out, "TURB ENG ITT:1", "celsius", engine.interstage_turbine_c,
        NameOrigin::documented, !piston);
    add(out, "TURB ENG MAX TORQUE PERCENT:1", "percent", engine.torque_pct,
        NameOrigin::documented, !piston);

    add(out, "ELECTRICAL MAIN BUS VOLTAGE:1", "volts", engine.bus_voltage_v,
        NameOrigin::confirmed_in_sdk);
    add(out, "ELECTRICAL GENALT BUS AMPS:1", "amperes", engine.alternator_current_a,
        NameOrigin::confirmed_in_sdk);

    const double fuel_gallons = truth.fuel_kg / fuel_density / litres_per_gallon;
    add(out, "FUEL TOTAL QUANTITY", "gallons", fuel_gallons,
        NameOrigin::confirmed_in_sdk);
    add(out, "FUEL LEFT QUANTITY", "gallons", fuel_gallons * 0.5,
        NameOrigin::documented);
    add(out, "FUEL RIGHT QUANTITY", "gallons", fuel_gallons * 0.5,
        NameOrigin::documented);
    add(out, "FUEL TOTAL QUANTITY WEIGHT", "kilograms", truth.fuel_kg,
        NameOrigin::documented);

    // --- Verita' del banco -------------------------------------------------
    //
    // Non esistono in nessun simulatore commerciale, e il prefisso lo dichiara.
    // Servono a chi giudica: la differenza fra queste e le omonime misurate e'
    // l'errore dello strumento, e un criterio di prova che non puo' calcolarla
    // non puo' nemmeno dire se l'EFIS ha sbagliato.
    add(out, "BENCH TRUTH AIRSPEED CALIBRATED", "knots",
        truth.calibrated_airspeed_m_s * knots_per_metre_per_second,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH PRESSURE ALTITUDE", "feet",
        Atmosphere::pressure_altitude_m(truth.static_pressure_pa) * feet_per_metre,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH ALTITUDE MSL", "feet", truth.altitude_m * feet_per_metre,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH VERTICAL SPEED", "feet per minute",
        truth.vertical_speed_m_s * feet_per_minute_per_metre_per_second,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH PITCH", "degrees", truth.pitch_rad * radians_to_degrees,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH BANK", "degrees", truth.roll_rad * radians_to_degrees,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH HEADING TRUE", "degrees",
        truth.heading_rad * radians_to_degrees, NameOrigin::bench_extension);
    add(out, "BENCH TRUTH LATITUDE", "degrees", truth.latitude_deg,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH LONGITUDE", "degrees", truth.longitude_deg,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH GROUND SPEED", "knots",
        truth.ground_speed_m_s * knots_per_metre_per_second,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH ALPHA", "degrees", truth.alpha_rad * radians_to_degrees,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH LIFT COEFFICIENT", "number", truth.lift_coefficient,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH MASS", "kilograms", truth.mass_kg,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH SHAFT POWER", "kilowatts", engine.shaft_power_kw,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH THRUST", "newtons", engine.thrust_n,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH PROPELLER EFFICIENCY", "number", engine.propeller_efficiency,
        NameOrigin::bench_extension);
    add(out, "BENCH TRUTH STALL SPEED", "knots",
        stall_speed_kcas(inputs.airframe, truth.mass_kg, inputs.controls.flap_deg),
        NameOrigin::bench_extension);
}

std::optional<double> find_sim_var(const std::vector<SimVarSample>& bus,
                                   std::string_view name) noexcept {
    const auto match = std::find_if(bus.begin(), bus.end(),
                                    [name](const SimVarSample& sample) {
                                        return sample.name == name;
                                    });
    if (match == bus.end() || !match->available) {
        return std::nullopt;
    }
    return match->value;
}

}  // namespace efis::fdm
