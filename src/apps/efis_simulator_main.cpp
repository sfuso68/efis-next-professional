#include "efis/simulation/system_simulator.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void usage() {
    std::cout << "EFIS NEXT complete system simulator (NOT FOR FLIGHT)\n"
              << "Usage: efis-simulator [--scenario NAME] [--step-ms N] [--duration-s N] [--output FILE]\n"
              << "Scenarios: full-flight, engine-emergency, electrical-failure, sensor-degradation\n";
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (const char c : value) {
        if (c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

void write_frame(std::ostream& out, const efis::simulation::SimulationFrame& frame) {
    out << std::fixed << std::setprecision(3)
        << "{\"schema\":\"efis-next.system-simulation.v1\",\"simulated\":true"
        << ",\"scenario\":\"" << efis::simulation::scenario_name(frame.scenario) << '"'
        << ",\"phase\":\"" << efis::simulation::phase_name(frame.phase) << '"'
        << ",\"time_ms\":" << frame.scenario_time.count()
        << ",\"progress\":" << frame.progress
        << ",\"flight\":{\"ias_kt\":" << frame.flight.air_data.indicated_airspeed_kt.value
        << ",\"altitude_ft\":" << frame.flight.air_data.pressure_altitude_ft.value
        << ",\"vs_fpm\":" << frame.flight.air_data.vertical_speed_fpm.value
        << ",\"heading_deg\":" << frame.flight.attitude.heading_deg.value
        << ",\"latitude\":" << frame.flight.position.latitude_deg.value
        << ",\"longitude\":" << frame.flight.position.longitude_deg.value << '}'
        << ",\"engine\":{\"rpm\":" << frame.engine.rpm.value
        << ",\"oil_pressure_psi\":" << frame.engine.oil_pressure_psi.value
        << ",\"oil_temperature_c\":" << frame.engine.oil_temperature_c.value
        << ",\"fuel_pressure_psi\":" << frame.engine.fuel_pressure_psi.value
        << ",\"bus_voltage_v\":" << frame.engine.bus_voltage_v.value
        << ",\"alternator_current_a\":" << frame.engine.alternator_current_a.value << '}'
        << ",\"airframe\":{\"master\":" << frame.airframe.master_on
        << ",\"engine_running\":" << frame.airframe.engine_running
        << ",\"alternator\":" << frame.airframe.alternator_on
        << ",\"fuel_pump\":" << frame.airframe.fuel_pump_on
        << ",\"pitot_heat\":" << frame.airframe.pitot_heat_on
        << ",\"gear_locked\":" << (frame.airframe.gear_nose_locked && frame.airframe.gear_left_locked && frame.airframe.gear_right_locked)
        << ",\"flaps_deg\":" << frame.airframe.flap_angle_deg << '}'
        << ",\"weather\":{\"category\":\"" << frame.weather.category
        << "\",\"radar\":" << frame.weather.radar_precipitation
        << ",\"intensity\":" << frame.weather.radar_intensity
        << ",\"metar\":\"" << json_escape(frame.weather.metar) << "\"}"
        << ",\"events\":[";
    for (std::size_t i = 0; i < frame.events.size(); ++i) {
        if (i != 0U) out << ',';
        out << '"' << json_escape(frame.events[i]) << '"';
    }
    out << "]}\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        auto scenario = efis::simulation::ScenarioId::full_flight;
        int step_ms = 1000;
        int duration_s = 600;
        std::string output_path;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") { usage(); return 0; }
            if (i + 1 >= argc) throw std::invalid_argument("missing option value");
            const std::string value = argv[++i];
            if (arg == "--scenario") scenario = efis::simulation::parse_scenario(value);
            else if (arg == "--step-ms") step_ms = std::stoi(value);
            else if (arg == "--duration-s") duration_s = std::stoi(value);
            else if (arg == "--output") output_path = value;
            else throw std::invalid_argument("unknown option: " + arg);
        }
        if (step_ms < 20 || duration_s < 0) throw std::invalid_argument("invalid simulation timing");
        std::ofstream file;
        std::ostream* destination = &std::cout;
        if (!output_path.empty()) {
            file.open(output_path, std::ios::trunc);
            if (!file) throw std::runtime_error("cannot open output file");
            destination = &file;
        }
        efis::simulation::SystemSimulator simulator{scenario};
        const auto start = efis::MonoClock::now();
        for (int elapsed_ms = 0; elapsed_ms <= duration_s * 1000; elapsed_ms += step_ms) {
            write_frame(*destination, simulator.sample(start + std::chrono::milliseconds{elapsed_ms},
                                                       std::chrono::milliseconds{elapsed_ms}));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "efis-simulator: " << error.what() << '\n';
        usage();
        return 2;
    }
}
