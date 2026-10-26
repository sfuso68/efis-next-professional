// Banco di volo EFIS NEXT (NON PER IL VOLO).
//
// Esegue il motore di volo a sei gradi di liberta', lo strato sensori e il bus
// delle variabili, e scrive una riga JSON per campione con la verita' del
// modello accanto a cio' che i sensori hanno misurato. Le due colonne insieme
// sono il motivo per cui questo programma esiste: un criterio di prova che
// voglia dire se l'EFIS ha sbagliato deve poter leggere entrambe.

#include "efis/fdm/airframe.hpp"
#include "efis/fdm/flight_bench.hpp"
#include "efis/fdm/scenario_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace efis;
using namespace efis::fdm;

constexpr double radians_to_degrees = 180.0 / std::numbers::pi;

enum class ScenarioId { cruise, takeoff, profile, stall };

void usage() {
    std::cout
        << "Banco di volo EFIS NEXT (NON PER IL VOLO)\n"
        << "Uso: efis-fdm [opzioni]\n"
        << "  --aircraft lsa|turboprop     cellula di riferimento (default lsa)\n"
        << "  --scenario NOME              cruise, takeoff, profile, stall\n"
        << "  --duration-s N               durata in secondi di simulazione (default 120)\n"
        << "  --rate-hz N                  cadenza di pubblicazione (default 20)\n"
        << "  --qnh N                       pressione al livello del mare in hPa (default 1013.25)\n"
        << "  --isa-offset N               scostamento di temperatura in kelvin (default 0)\n"
        << "  --wind DIR/VEL[/RAFFICA]     vento da DIR gradi, in nodi\n"
        << "  --fail NOME                  guasto sensore da iniettare\n"
        << "  --fail-engine NOME           guasto propulsore da iniettare\n"
        << "  --fail-at N                  istante di iniezione in secondi (default 0)\n"
        << "  --output FILE                scrive le righe JSON su file invece che a video\n"
        << "  --list-vars                  elenca il bus delle variabili ed esce\n"
        << "Guasti sensore: pitot-blocked, pitot-leak, static-blocked, ahrs-latched,\n"
        << "                ahrs-drift, gnss-outage, gnss-degraded, magnetometer-bias,\n"
        << "                oat-stuck\n"
        << "Guasti propulsore: fuel-pump, oil-loss, alternator, single-cylinder, flameout\n";
}

[[nodiscard]] ScenarioId parse_scenario(std::string_view name) {
    if (name == "cruise") return ScenarioId::cruise;
    if (name == "takeoff") return ScenarioId::takeoff;
    if (name == "profile") return ScenarioId::profile;
    if (name == "stall") return ScenarioId::stall;
    throw std::runtime_error("scenario sconosciuto");
}

[[nodiscard]] const char* scenario_name(ScenarioId scenario) noexcept {
    switch (scenario) {
        case ScenarioId::cruise: return "cruise";
        case ScenarioId::takeoff: return "takeoff";
        case ScenarioId::profile: return "profile";
        case ScenarioId::stall: return "stall";
    }
    return "cruise";
}

[[nodiscard]] SensorFault parse_sensor_fault(std::string_view name) {
    if (name == "none") return SensorFault::none;
    if (name == "pitot-blocked") return SensorFault::pitot_blocked;
    if (name == "pitot-leak") return SensorFault::pitot_leak;
    if (name == "static-blocked") return SensorFault::static_blocked;
    if (name == "ahrs-latched") return SensorFault::ahrs_latched;
    if (name == "ahrs-drift") return SensorFault::ahrs_drift;
    if (name == "gnss-outage") return SensorFault::gnss_outage;
    if (name == "gnss-degraded") return SensorFault::gnss_degraded;
    if (name == "magnetometer-bias") return SensorFault::magnetometer_bias;
    if (name == "oat-stuck") return SensorFault::oat_stuck;
    throw std::runtime_error("guasto sensore sconosciuto");
}

[[nodiscard]] PropulsionFault parse_engine_fault(std::string_view name) {
    if (name == "none") return PropulsionFault::none;
    if (name == "fuel-pump") return PropulsionFault::fuel_pump;
    if (name == "oil-loss") return PropulsionFault::oil_loss;
    if (name == "alternator") return PropulsionFault::alternator;
    if (name == "single-cylinder") return PropulsionFault::single_cylinder;
    if (name == "flameout") return PropulsionFault::flameout;
    throw std::runtime_error("guasto propulsore sconosciuto");
}

// Profilo dello scenario: decide gli obiettivi del pilota in funzione del
// tempo, e nulla di piu'. Tenere il profilo separato dal pilota permette di
// aggiungere uno scenario senza toccare il controllo, e di correggere il
// controllo senza riscrivere gli scenari.
struct ScenarioProfile final {
    ScenarioId scenario{ScenarioId::cruise};
    double field_elevation_ft{};
    double cruise_altitude_ft{};
    double cruise_airspeed_kcas{};
    double initial_heading_deg{};
    double rotation_kcas{};
    bool airborne_start{true};
};

[[nodiscard]] ScenarioProfile build_profile(ScenarioId scenario,
                                            const Airframe& airframe,
                                            double field_elevation_ft) {
    ScenarioProfile profile{};
    profile.scenario = scenario;
    profile.field_elevation_ft = field_elevation_ft;
    profile.initial_heading_deg = 40.0;

    // La velocita' di crociera e' il novanta per cento della VNO e la
    // rotazione il dieci per cento sopra la velocita' di stallo pulita:
    // entrambe discendono dai limiti della cellula invece di essere scritte
    // qui, cosi' una cellula nuova non porta con se' uno scenario sbagliato.
    profile.cruise_airspeed_kcas = airframe.limits.normal_operating_kcas * 0.90;
    profile.rotation_kcas = airframe.limits.stall_clean_kcas * 1.10;
    profile.cruise_altitude_ft = airframe.category == Category::professional
                                     ? 16000.0
                                     : 3000.0;
    profile.airborne_start = scenario != ScenarioId::takeoff;
    if (scenario == ScenarioId::stall) {
        profile.cruise_altitude_ft = airframe.category == Category::professional
                                         ? 10000.0
                                         : 4000.0;
    }
    return profile;
}

// Obiettivi del pilota, piu' i comandi che il profilo deve dare direttamente al
// modello: l'avviamento del motore e i freni non sono obiettivi di volo.
struct ProfileCommand final {
    PilotTargets targets{};
    bool starter{};
    bool magnetos{true};
    bool fuel_pump{true};
    double brake{};
    std::string phase{"cruise"};
};

[[nodiscard]] ProfileCommand command_at(const ScenarioProfile& profile,
                                        const Airframe& airframe,
                                        const VehicleState& truth,
                                        double time_s) {
    ProfileCommand command{};
    command.targets.heading_deg = profile.initial_heading_deg;
    command.targets.airspeed_kcas = profile.cruise_airspeed_kcas;
    command.targets.altitude_ft = profile.cruise_altitude_ft;
    command.targets.gear_down = !airframe.gear.retractable;

    const double altitude_ft = truth.altitude_m * feet_per_metre;
    const double calibrated_kt = truth.calibrated_airspeed_m_s *
                                 knots_per_metre_per_second;

    switch (profile.scenario) {
        case ScenarioId::cruise:
            command.phase = "cruise";
            break;

        case ScenarioId::takeoff:
            if (time_s < 8.0) {
                // Avviamento: magneti, pompa, avviatore e freni inseriti.
                command.phase = "start";
                command.starter = !truth.on_ground || true;
                command.brake = 1.0;
                command.targets.airspeed_kcas = 0.0;
                command.targets.altitude_ft = profile.field_elevation_ft;
                command.targets.on_runway = true;
                command.targets.gear_down = true;
                break;
            }
            command.targets.gear_down = true;
            if (calibrated_kt < profile.rotation_kcas) {
                command.phase = "roll";
                command.targets.on_runway = true;
                command.targets.airspeed_kcas = profile.rotation_kcas * 1.3;
                command.targets.altitude_ft = profile.field_elevation_ft;
                break;
            }
            if (altitude_ft < profile.field_elevation_ft + 1500.0) {
                command.phase = "climb";
                command.targets.airspeed_kcas = airframe.limits.best_climb_kcas;
                command.targets.altitude_ft = profile.field_elevation_ft + 1500.0;
                command.targets.max_vertical_speed_fpm = 900.0;

                // Carrello retratto solo dopo cento piedi dal suolo: prima si
                // tiene, perche' un riattaccato con il carrello in movimento e'
                // la situazione che il manuale vuole evitare.
                command.targets.gear_down = altitude_ft <
                                            profile.field_elevation_ft + 100.0;
                break;
            }
            command.phase = "level";
            command.targets.altitude_ft = profile.field_elevation_ft + 1500.0;
            command.targets.gear_down = !airframe.gear.retractable;
            break;

        case ScenarioId::profile:
            // Salita, due virate e discesa: il profilo che esercita insieme
            // nastro delle quote, prua, rateo di virata e pallina.
            if (time_s < 60.0) {
                command.phase = "climb";
                command.targets.altitude_ft = profile.cruise_altitude_ft + 2000.0;
                command.targets.airspeed_kcas = airframe.limits.best_climb_kcas;
            } else if (time_s < 120.0) {
                command.phase = "turn-right";
                command.targets.altitude_ft = profile.cruise_altitude_ft + 2000.0;
                command.targets.heading_deg = profile.initial_heading_deg + 90.0;
            } else if (time_s < 180.0) {
                command.phase = "turn-left";
                command.targets.altitude_ft = profile.cruise_altitude_ft + 2000.0;
                command.targets.heading_deg = profile.initial_heading_deg - 60.0;
            } else {
                command.phase = "descent";
                command.targets.altitude_ft = profile.cruise_altitude_ft;
                command.targets.heading_deg = profile.initial_heading_deg - 60.0;
                command.targets.max_vertical_speed_fpm = 600.0;
            }
            break;

        case ScenarioId::stall:
            if (time_s < 15.0) {
                command.phase = "stabilise";
                break;
            }
            if (!truth.stalled && altitude_ft > profile.cruise_altitude_ft - 500.0) {
                command.phase = "stall-entry";
                command.targets.stall_entry = true;
                break;
            }
            command.phase = "recovery";
            command.targets.altitude_ft = profile.cruise_altitude_ft - 700.0;
            break;
    }
    return command;
}

void write_row(std::ostream& out, const FlightBench& bench, const ScenarioProfile& profile,
               const ProfileCommand& command, SensorFault sensor_fault,
               PropulsionFault engine_fault, double time_s) {
    const VehicleState& truth = bench.vehicle().state();
    const IndicatedState& indicated = bench.sensors().indicated();
    const PropulsionState& engine = bench.vehicle().engine();
    const Controls& controls = bench.vehicle().controls();

    out << std::fixed << std::setprecision(3)
        << "{\"schema\":\"efis-next.fdm.v1\",\"simulated\":true"
        << ",\"scenario\":\"" << scenario_name(profile.scenario) << '"'
        << ",\"phase\":\"" << command.phase << '"'
        << ",\"t_s\":" << time_s
        << ",\"sensor_fault\":\"" << sensor_fault_name(sensor_fault) << '"'
        << ",\"engine_fault\":\"" << propulsion_fault_name(engine_fault) << '"'
        << ",\"truth\":{"
        << "\"cas_kt\":" << truth.calibrated_airspeed_m_s * knots_per_metre_per_second
        << ",\"tas_kt\":" << truth.true_airspeed_m_s * knots_per_metre_per_second
        << ",\"altitude_ft\":" << truth.altitude_m * feet_per_metre
        << ",\"vs_fpm\":" << truth.vertical_speed_m_s *
                                 feet_per_minute_per_metre_per_second
        << ",\"pitch_deg\":" << truth.pitch_rad * radians_to_degrees
        << ",\"roll_deg\":" << truth.roll_rad * radians_to_degrees
        << ",\"heading_deg\":" << truth.heading_rad * radians_to_degrees
        << ",\"alpha_deg\":" << truth.alpha_rad * radians_to_degrees
        << ",\"mach\":" << truth.mach
        << ",\"gs_kt\":" << truth.ground_speed_m_s * knots_per_metre_per_second
        << ",\"load_factor\":" << truth.load_factor
        << ",\"lift_coefficient\":" << truth.lift_coefficient
        << ",\"mass_kg\":" << truth.mass_kg
        << ",\"on_ground\":" << (truth.on_ground ? 1 : 0)
        << ",\"stalled\":" << (truth.stalled ? 1 : 0)
        << ",\"lat\":" << std::setprecision(6) << truth.latitude_deg
        << ",\"lon\":" << truth.longitude_deg << std::setprecision(3) << '}'
        << ",\"indicated\":{"
        << "\"ias_kt\":" << indicated.indicated_airspeed_kt
        << ",\"pressure_altitude_ft\":" << indicated.pressure_altitude_ft
        << ",\"indicated_altitude_ft\":" << indicated.indicated_altitude_ft
        << ",\"baro_hpa\":" << indicated.baro_setting_hpa
        << ",\"vs_fpm\":" << indicated.vertical_speed_fpm
        << ",\"pitch_deg\":" << indicated.pitch_deg
        << ",\"roll_deg\":" << indicated.roll_deg
        << ",\"heading_mag_deg\":" << indicated.magnetic_heading_deg
        << ",\"turn_rate_deg_s\":" << indicated.turn_rate_deg_s
        << ",\"lateral_g\":" << indicated.lateral_acceleration_g
        << ",\"normal_g\":" << indicated.normal_acceleration_g
        << ",\"aoa_deg\":" << indicated.angle_of_attack_deg
        << ",\"tat_c\":" << indicated.total_air_temperature_c
        << ",\"mach\":" << indicated.mach
        << ",\"gnss_valid\":" << (indicated.gnss_valid ? 1 : 0)
        << ",\"satellites\":" << indicated.satellites
        << ",\"gs_kt\":" << indicated.ground_speed_kt
        << ",\"track_deg\":" << indicated.track_deg
        << ",\"gnss_vs_fpm\":" << indicated.gnss_vertical_speed_fpm
        << ",\"lat\":" << std::setprecision(6) << indicated.latitude_deg
        << ",\"lon\":" << indicated.longitude_deg << std::setprecision(3) << '}'
        << ",\"engine\":{"
        << "\"running\":" << (engine.running ? 1 : 0)
        << ",\"rpm\":" << engine.engine_rpm
        << ",\"prop_rpm\":" << engine.propeller_rpm
        << ",\"shaft_kw\":" << engine.shaft_power_kw
        << ",\"thrust_n\":" << engine.thrust_n
        << ",\"map_inhg\":" << engine.manifold_pressure_inhg
        << ",\"oil_psi\":" << engine.oil_pressure_psi
        << ",\"oil_c\":" << engine.oil_temperature_c
        << ",\"fuel_psi\":" << engine.fuel_pressure_psi
        << ",\"fuel_flow_kg_h\":" << engine.fuel_flow_kg_s * 3600.0
        << ",\"cht1_c\":" << engine.cylinder_head_c[0]
        << ",\"egt1_c\":" << engine.exhaust_gas_c[0]
        << ",\"egt2_c\":" << engine.exhaust_gas_c[1]
        << ",\"itt_c\":" << engine.interstage_turbine_c
        << ",\"n1_pct\":" << engine.gas_generator_pct
        << ",\"torque_pct\":" << engine.torque_pct
        << ",\"bus_v\":" << engine.bus_voltage_v
        << ",\"alt_a\":" << engine.alternator_current_a << '}'
        << ",\"controls\":{"
        << "\"elevator\":" << controls.elevator
        << ",\"aileron\":" << controls.aileron
        << ",\"rudder\":" << controls.rudder
        << ",\"trim\":" << controls.elevator_trim
        << ",\"throttle\":" << controls.throttle
        << ",\"flap_deg\":" << controls.flap_deg
        << ",\"gear_down\":" << (controls.gear_down ? 1 : 0)
        << ",\"brake\":" << controls.brake << "}}\n";
}

void list_variables(const Airframe& airframe) {
    Environment environment{};
    environment.field_elevation_m = 100.0;
    FlightBench bench(airframe, environment);
    bench.vehicle().place_in_flight(45.6, 12.2, 3000.0, 40.0,
                                    airframe.limits.normal_operating_kcas * 0.9);
    bench.align_sensors();
    bench.step_seconds(1.0);

    std::vector<SimVarSample> bus;
    bench.publish_bus(bus);

    std::cout << "Bus delle variabili, cellula " << airframe.name << " ("
              << category_name(airframe.category) << ")\n"
              << "Provenienza del nome: SDK = trovato nei ModelBehaviorDefs "
                 "dell'SDK, DOC = variabile documentata, BENCH = estensione del "
                 "banco\n\n";
    for (const SimVarSample& sample : bus) {
        std::cout << std::left << std::setw(6) << name_origin_label(sample.origin)
                  << std::setw(44) << sample.name << std::setw(20) << sample.unit;
        if (sample.available) {
            std::cout << std::right << std::fixed << std::setprecision(3) << std::setw(12)
                      << sample.value;
        } else {
            std::cout << std::right << std::setw(12) << "assente";
        }
        std::cout << '\n';
    }
    std::cout << "\n" << bus.size() << " variabili\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string aircraft = "lsa";
    std::string scenario_text = "cruise";
    std::string output_path;
    std::string sensor_fault_text = "none";
    std::string engine_fault_text = "none";
    double duration_s = 120.0;
    double rate_hz = 20.0;
    double qnh_hpa = 1013.25;
    double isa_offset_k = 0.0;
    double fail_at_s = 0.0;
    double wind_from_deg = 0.0;
    double wind_speed_kt = 0.0;
    double wind_gust_kt = 0.0;
    bool list_vars = false;

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            const auto value = [&]() -> std::string {
                if (index + 1 >= argc) {
                    throw std::runtime_error("manca il valore dell'opzione");
                }
                return argv[++index];
            };
            if (argument == "--help" || argument == "-h") {
                usage();
                return EXIT_SUCCESS;
            } else if (argument == "--aircraft") {
                aircraft = value();
            } else if (argument == "--scenario") {
                scenario_text = value();
            } else if (argument == "--duration-s") {
                duration_s = std::stod(value());
            } else if (argument == "--rate-hz") {
                rate_hz = std::stod(value());
            } else if (argument == "--qnh") {
                qnh_hpa = std::stod(value());
            } else if (argument == "--isa-offset") {
                isa_offset_k = std::stod(value());
            } else if (argument == "--fail") {
                sensor_fault_text = value();
            } else if (argument == "--fail-engine") {
                engine_fault_text = value();
            } else if (argument == "--fail-at") {
                fail_at_s = std::stod(value());
            } else if (argument == "--output") {
                output_path = value();
            } else if (argument == "--list-vars") {
                list_vars = true;
            } else if (argument == "--wind") {
                std::stringstream stream(value());
                std::string field;
                int field_index = 0;
                while (std::getline(stream, field, '/')) {
                    const double parsed = std::stod(field);
                    if (field_index == 0) {
                        wind_from_deg = parsed;
                    } else if (field_index == 1) {
                        wind_speed_kt = parsed;
                    } else if (field_index == 2) {
                        wind_gust_kt = parsed;
                    }
                    ++field_index;
                }
            } else {
                usage();
                return EXIT_FAILURE;
            }
        }

        const Airframe airframe = aircraft == "turboprop" ? professional_airframe()
                                                          : light_sport_airframe();
        if (aircraft != "turboprop" && aircraft != "lsa") {
            throw std::runtime_error("cellula sconosciuta");
        }

        if (list_vars) {
            list_variables(airframe);
            return EXIT_SUCCESS;
        }

        const ScenarioId scenario = parse_scenario(scenario_text);
        const SensorFault sensor_fault = parse_sensor_fault(sensor_fault_text);
        const PropulsionFault engine_fault = parse_engine_fault(engine_fault_text);

        Environment environment{};
        environment.atmosphere.sea_level_pressure_pa = qnh_hpa * pascal_per_hectopascal;
        environment.atmosphere.temperature_offset_k = isa_offset_k;
        environment.wind.from_deg = wind_from_deg;
        environment.wind.speed_kt = wind_speed_kt;
        environment.wind.gust_kt = wind_gust_kt;

        // Treviso, quota di campo reale: lo scenario del simulatore di sistema
        // parte da li', e due banchi che partono dallo stesso campo si possono
        // confrontare.
        environment.field_elevation_m = 18.0;
        environment.magnetic_variation_deg = 3.4;

        const ScenarioProfile profile = build_profile(scenario, airframe,
                                                      environment.field_elevation_m *
                                                          feet_per_metre);

        FlightBench bench(airframe, environment);
        if (profile.airborne_start) {
            bench.vehicle().place_in_flight(45.648, 12.194, profile.cruise_altitude_ft,
                                            profile.initial_heading_deg,
                                            profile.cruise_airspeed_kcas);
        } else {
            bench.vehicle().place_on_ground(45.648, 12.194, profile.initial_heading_deg);
        }
        bench.align_sensors();

        // La regolazione dell'altimetro e' quella del giorno, non la standard:
        // partire in standard con un QNH diverso darebbe una quota indicata
        // sbagliata dall'inizio, e nasconderebbe l'effetto che si vuole vedere
        // quando la regolazione la si sbaglia per prova.
        bench.sensors().set_baro_setting_hpa(qnh_hpa);

        ScenarioPilot pilot(bench.vehicle().airframe());

        std::ofstream file;
        if (!output_path.empty()) {
            file.open(output_path, std::ios::out | std::ios::trunc);
            if (!file) {
                throw std::runtime_error("impossibile scrivere il file di uscita");
            }
        }
        std::ostream& out = output_path.empty() ? std::cout : file;

        const double publish_step_s = 1.0 / std::max(rate_hz, 1.0);

        // Cadenza del controllo, separata da quella di pubblicazione.
        //
        // Venti millisecondi. La separazione non e' un dettaglio di
        // implementazione: il pilota chiuse a cinque hertz, cioe' alla cadenza
        // con cui si pubblica per una registrazione lunga, oscilla da fondo
        // corsa a fondo corsa, perche' il periodo del modo di corto periodo di
        // un LSA e' di un paio di secondi e un comando tenuto fermo per due
        // decimi introduce abbastanza ritardo di fase da invertire il segno
        // della correzione. La prima versione del banco lo faceva, e la
        // crociera perdeva quattrocento piedi al minuto con i comandi a
        // sbattere. Chi pubblica piano non deve per questo pilotare piano.
        const double control_step_s = std::min(publish_step_s, 0.02);
        bool faults_injected = false;
        ProfileCommand command{};

        for (double time_s = 0.0; time_s <= duration_s; time_s += publish_step_s) {
            if (!faults_injected && time_s >= fail_at_s) {
                bench.sensors().set_fault(sensor_fault);
                bench.vehicle().propulsion().set_fault(engine_fault);
                faults_injected = true;
            }

            double consumed_s = 0.0;
            while (consumed_s < publish_step_s - 1.0e-9) {
                const double slice_s = std::min(control_step_s,
                                                publish_step_s - consumed_s);
                command = command_at(profile, airframe, bench.vehicle().state(),
                                     time_s + consumed_s);
                pilot.set_targets(command.targets);
                pilot.fly(bench.vehicle(), slice_s);

                // I comandi che non sono di volo li scrive il profilo, dopo il
                // pilota: l'avviamento e i freni non appartengono al controllo
                // di traiettoria, e lasciarli al pilota avrebbe significato
                // dargli autorita' su cose che un pilota di scenario non deve
                // avere.
                Controls controls = bench.vehicle().controls();
                controls.starter = command.starter;
                controls.magnetos_on = command.magnetos;
                controls.fuel_pump_on = command.fuel_pump;
                controls.brake = command.brake;
                bench.vehicle().set_controls(controls);

                bench.step_seconds(slice_s);
                consumed_s += slice_s;
            }
            write_row(out, bench, profile, command, sensor_fault, engine_fault, time_s);
        }
        out.flush();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "errore: " << error.what() << '\n';
        usage();
        return EXIT_FAILURE;
    }
}
