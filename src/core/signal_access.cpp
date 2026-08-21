#include "efis/core/signal_access.hpp"

#include <array>

// La corrispondenza e l'elenco delle grandezze disponibili sono due cose che
// devono restare d'accordo: se divergessero, il menu offrirebbe strumenti che
// nessuno sa alimentare, oppure nasconderebbe grandezze che il sistema produce.
// Non c'e' un meccanismo che lo garantisca a compilazione, quindi lo garantisce
// una prova: ogni voce dell'elenco deve risolvere, e ogni grandezza risolta
// deve comparire nell'elenco.

namespace efis {

const Sample<double>* find_signal(const FlightSnapshot& snapshot,
                                  SignalId signal) noexcept {
    switch (signal) {
        case SignalId::roll: return &snapshot.attitude.roll_deg;
        case SignalId::pitch: return &snapshot.attitude.pitch_deg;
        case SignalId::heading: return &snapshot.attitude.heading_deg;
        case SignalId::turn_rate: return &snapshot.attitude.turn_rate_deg_s;
        case SignalId::lateral_acceleration:
            return &snapshot.attitude.lateral_acceleration_g;

        case SignalId::indicated_airspeed:
            return &snapshot.air_data.indicated_airspeed_kt;
        case SignalId::pressure_altitude:
            return &snapshot.air_data.pressure_altitude_ft;
        case SignalId::vertical_speed: return &snapshot.air_data.vertical_speed_fpm;
        case SignalId::baro_setting: return &snapshot.air_data.baro_setting_hpa;
        case SignalId::indicated_altitude:
            return &snapshot.air_data.indicated_altitude_ft;

        case SignalId::latitude: return &snapshot.position.latitude_deg;
        case SignalId::longitude: return &snapshot.position.longitude_deg;
        case SignalId::ground_speed: return &snapshot.position.ground_speed_kt;
        case SignalId::gnss_vertical_speed:
            return &snapshot.position.gnss_vertical_speed_fpm;

        case SignalId::commanded_roll: return &snapshot.guidance.commanded_roll_deg;
        case SignalId::commanded_pitch: return &snapshot.guidance.commanded_pitch_deg;
        case SignalId::selected_altitude:
            return &snapshot.guidance.selected_altitude_ft;
        case SignalId::selected_course: return &snapshot.guidance.selected_course_deg;
        case SignalId::selected_airspeed:
            return &snapshot.guidance.selected_airspeed_kt;

        case SignalId::cdi_deviation: return &snapshot.navigation.cdi_deviation_dots;
        case SignalId::glide_slope_deviation:
            return &snapshot.navigation.glide_slope_dots;

        default: return nullptr;
    }
}

const Sample<double>* find_signal(const engine::EngineSnapshot& snapshot,
                                  SignalId signal) noexcept {
    switch (signal) {
        case SignalId::engine_rpm: return &snapshot.rpm;
        case SignalId::manifold_pressure: return &snapshot.manifold_pressure_inhg;
        case SignalId::oil_pressure: return &snapshot.oil_pressure_psi;
        case SignalId::oil_temperature: return &snapshot.oil_temperature_c;
        case SignalId::fuel_pressure: return &snapshot.fuel_pressure_psi;
        case SignalId::fuel_flow: return &snapshot.fuel_flow_gph;
        case SignalId::fuel_quantity_left: return &snapshot.fuel_left_gal;
        case SignalId::fuel_quantity_right: return &snapshot.fuel_right_gal;
        case SignalId::bus_voltage: return &snapshot.bus_voltage_v;
        case SignalId::alternator_current: return &snapshot.alternator_current_a;

        case SignalId::cylinder_head_temperature_1: return &snapshot.cht_c[0];
        case SignalId::cylinder_head_temperature_2: return &snapshot.cht_c[1];
        case SignalId::cylinder_head_temperature_3: return &snapshot.cht_c[2];
        case SignalId::cylinder_head_temperature_4: return &snapshot.cht_c[3];

        case SignalId::exhaust_gas_temperature_1: return &snapshot.egt_c[0];
        case SignalId::exhaust_gas_temperature_2: return &snapshot.egt_c[1];
        case SignalId::exhaust_gas_temperature_3: return &snapshot.egt_c[2];
        case SignalId::exhaust_gas_temperature_4: return &snapshot.egt_c[3];

        default: return nullptr;
    }
}

std::span<const SignalId> flight_signals() noexcept {
    static constexpr std::array<SignalId, 21> list{
        SignalId::roll,
        SignalId::pitch,
        SignalId::heading,
        SignalId::turn_rate,
        SignalId::lateral_acceleration,
        SignalId::indicated_airspeed,
        SignalId::pressure_altitude,
        SignalId::vertical_speed,
        SignalId::baro_setting,
        SignalId::indicated_altitude,
        SignalId::latitude,
        SignalId::longitude,
        SignalId::ground_speed,
        SignalId::gnss_vertical_speed,
        SignalId::commanded_roll,
        SignalId::commanded_pitch,
        SignalId::selected_altitude,
        SignalId::selected_course,
        SignalId::selected_airspeed,
        SignalId::cdi_deviation,
        SignalId::glide_slope_deviation,
    };
    return list;
}

std::span<const SignalId> engine_signals() noexcept {
    static constexpr std::array<SignalId, 18> list{
        SignalId::engine_rpm,
        SignalId::manifold_pressure,
        SignalId::oil_pressure,
        SignalId::oil_temperature,
        SignalId::fuel_pressure,
        SignalId::fuel_flow,
        SignalId::fuel_quantity_left,
        SignalId::fuel_quantity_right,
        SignalId::bus_voltage,
        SignalId::alternator_current,
        SignalId::cylinder_head_temperature_1,
        SignalId::cylinder_head_temperature_2,
        SignalId::cylinder_head_temperature_3,
        SignalId::cylinder_head_temperature_4,
        SignalId::exhaust_gas_temperature_1,
        SignalId::exhaust_gas_temperature_2,
        SignalId::exhaust_gas_temperature_3,
        SignalId::exhaust_gas_temperature_4,
    };
    return list;
}

}  // namespace efis
