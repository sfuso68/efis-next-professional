#include "efis/simulation/mission_simulator.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::int64_t number(const std::string& text) {
    std::int64_t n{};
    const auto r = std::from_chars(text.data(), text.data() + text.size(), n);
    if (r.ec != std::errc{} || r.ptr != text.data() + text.size())
        throw std::invalid_argument("invalid integer option");
    return n;
}
double real_number(const std::string& text) {
    double n{};
    const auto r = std::from_chars(text.data(), text.data() + text.size(), n);
    if (r.ec != std::errc{} || r.ptr != text.data() + text.size())
        throw std::invalid_argument("invalid numeric profile option");
    return n;
}
void value(std::ostream& out, const char* name, const efis::Sample<double>& sample,
           bool comma = true) {
    if (comma)
        out << ',';
    out << '"' << name << "\":{\"value\":" << sample.value << ",\"raw\":" << sample.raw
        << ",\"validity\":" << static_cast<unsigned>(sample.validity)
        << ",\"signal\":" << static_cast<unsigned>(sample.origin.signal)
        << ",\"unit\":" << static_cast<unsigned>(sample.origin.unit) << ",\"simulated\":true}";
}
void write(const efis::simulation::SimulationFrame& f,
           const efis::simulation::MissionSimulator& sim) {
    std::cout << std::setprecision(12)
              << "{\"schema\":\"efis-next.mission-sil.v1\",\"simulated\":true,\"flight_approval\":"
                 "false,\"profile_status\":\"SYNTHETIC_UNVALIDATED\",\"profile\":\""
              << sim.profile().id << "\",\"scenario\":\""
              << efis::simulation::scenario_name(f.scenario) << "\",\"phase\":\""
              << efis::simulation::phase_name(f.phase)
              << "\",\"time_ms\":" << f.scenario_time.count()
              << ",\"sequence\":" << f.flight.sequence
              << ",\"duration_ms\":" << sim.duration().count() << ",\"flight\":{";
    value(std::cout, "ias_kt", f.flight.air_data.indicated_airspeed_kt, false);
    value(std::cout, "altitude_ft", f.flight.air_data.indicated_altitude_ft);
    value(std::cout, "pressure_altitude_ft", f.flight.air_data.pressure_altitude_ft);
    value(std::cout, "vs_fpm", f.flight.air_data.vertical_speed_fpm);
    value(std::cout, "qnh_hpa", f.flight.air_data.baro_setting_hpa);
    value(std::cout, "latitude", f.flight.position.latitude_deg);
    value(std::cout, "longitude", f.flight.position.longitude_deg);
    value(std::cout, "gs_kt", f.flight.position.ground_speed_kt);
    value(std::cout, "heading_deg", f.flight.attitude.heading_deg);
    value(std::cout, "pitch_deg", f.flight.attitude.pitch_deg);
    value(std::cout, "roll_deg", f.flight.attitude.roll_deg);
    std::cout << "},\"engine\":{";
    value(std::cout, "rpm", f.engine.rpm, false);
    value(std::cout, "map_inhg", f.engine.manifold_pressure_inhg);
    value(std::cout, "oil_pressure_psi", f.engine.oil_pressure_psi);
    value(std::cout, "oil_temperature_c", f.engine.oil_temperature_c);
    value(std::cout, "fuel_pressure_psi", f.engine.fuel_pressure_psi);
    value(std::cout, "flow_gph", f.engine.fuel_flow_gph);
    value(std::cout, "left_gal", f.engine.fuel_left_gal);
    value(std::cout, "right_gal", f.engine.fuel_right_gal);
    value(std::cout, "bus_v", f.engine.bus_voltage_v);
    value(std::cout, "current_a", f.engine.alternator_current_a);
    for (std::size_t i = 0; i < 4; ++i) {
        const auto name = "cht" + std::to_string(i + 1);
        value(std::cout, name.c_str(), f.engine.cht_c[i]);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        const auto name = "egt" + std::to_string(i + 1);
        value(std::cout, name.c_str(), f.engine.egt_c[i]);
    }
    std::cout << "},\"airframe\":{\"gear_nose_locked\":"
              << (f.airframe.gear_nose_locked ? "true" : "false")
              << ",\"gear_left_locked\":" << (f.airframe.gear_left_locked ? "true" : "false")
              << ",\"gear_right_locked\":" << (f.airframe.gear_right_locked ? "true" : "false")
              << ",\"flaps_measured_deg\":" << f.airframe.flap_angle_deg
              << ",\"flaps_commanded_deg\":" << f.airframe.flap_command_deg
              << ",\"engine_running\":" << (f.airframe.engine_running ? "true" : "false")
              << ",\"alternator_on\":" << (f.airframe.alternator_on ? "true" : "false") << "}}\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        efis::simulation::ScenarioId scenario = efis::simulation::ScenarioId::full_flight;
        std::int64_t step = 1000, seed = 20261024;
        bool describe = false;
        efis::simulation::MissionProfile profile;
        const std::array<std::pair<std::string, double*>, 18> parameters{
            {{"--departure-lat", &profile.departure_lat},
             {"--departure-lon", &profile.departure_lon},
             {"--departure-ft", &profile.departure_ft},
             {"--destination-lat", &profile.destination_lat},
             {"--destination-lon", &profile.destination_lon},
             {"--destination-ft", &profile.destination_ft},
             {"--cruise-ft", &profile.cruise_ft},
             {"--climb-fpm", &profile.climb_fpm},
             {"--descent-fpm", &profile.descent_fpm},
             {"--cruise-gs-kt", &profile.cruise_ground_kt},
             {"--wind-north-kt", &profile.wind_north_kt},
             {"--wind-east-kt", &profile.wind_east_kt},
             {"--ambient-c", &profile.ambient_c},
             {"--fuel-left-gal", &profile.fuel_left_gal},
             {"--fuel-right-gal", &profile.fuel_right_gal},
             {"--maximum-flow-gph", &profile.maximum_flow_gph},
             {"--battery-ah", &profile.battery_ah},
             {"--load-a", &profile.load_a}}};
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "efis-mission-simulator --scenario NAME --step-ms 20..60000 --seed N "
                             "[--describe]\nOffline JSONL only. SYNTHETIC_UNVALIDATED, NOT FOR "
                             "FLIGHT. No sockets or actuators.\nNumeric profile overrides:\n";
                for (const auto& parameter : parameters)
                    std::cout << parameter.first << " N\n";
                return 0;
            }
            if (option == "--describe") {
                describe = true;
                continue;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument("missing option value");
            const std::string arg = argv[++i];
            if (option == "--scenario")
                scenario = efis::simulation::parse_scenario(arg);
            else if (option == "--step-ms")
                step = number(arg);
            else if (option == "--seed")
                seed = number(arg);
            else {
                const auto field = std::find_if(parameters.begin(), parameters.end(),
                                                [&](const auto& p) { return p.first == option; });
                if (field == parameters.end())
                    throw std::invalid_argument("unknown option");
                *field->second = real_number(arg);
            }
        }
        if (step < 20 || step > 60000 || seed < 0)
            throw std::invalid_argument("option outside supported range");
        efis::simulation::MissionSimulator sim{profile, scenario, static_cast<std::uint64_t>(seed)};
        if (describe) {
            std::cout << std::setprecision(17)
                      << "{\"schema\":\"efis-next.mission-profile-description.v1\",\"profile\":\""
                      << sim.profile().id
                      << "\",\"profile_status\":\"SYNTHETIC_UNVALIDATED\",\"duration_ms\":"
                      << sim.duration().count()
                      << ",\"network\":false,\"flight_approval\":false,\"seed\":" << seed
                      << ",\"parameters\":{";
            bool first = true;
            for (const auto& p : parameters) {
                if (!first)
                    std::cout << ',';
                first = false;
                std::cout << '"' << p.first.substr(2) << "\":" << *p.second;
            }
            std::cout << "}}\n";
            return 0;
        }
        const efis::MonoTime epoch{};
        for (std::int64_t time = 0;; time = std::min(time + step, sim.duration().count())) {
            write(sim.sample(epoch + std::chrono::milliseconds{time},
                             std::chrono::milliseconds{time}),
                  sim);
            if (time == sim.duration().count())
                break;
        }
        if (!std::cout)
            throw std::runtime_error("output write failed");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "efis-mission-simulator: " << e.what() << '\n';
        return 2;
    }
}
