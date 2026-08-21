#include "efis/core/alert_engine.hpp"

#include <algorithm>
#include <string>

namespace efis {
namespace {

int priority(AlertSeverity severity) {
    return static_cast<int>(severity);
}

bool is_simulated(const Sample<double>& sample) {
    return sample.validity == Validity::simulated;
}

}  // namespace

AlertState AlertEngine::evaluate(const FlightSnapshot& s) const {
    AlertState result;

    if (!s.air_data.indicated_airspeed_kt.usable()) {
        result.active.push_back({"AIRSPEED_FAIL", "AIRSPEED FAIL",
                                 AlertSeverity::warning, false});
    }
    if (!s.air_data.pressure_altitude_ft.usable()) {
        result.active.push_back({"ALTITUDE_FAIL", "ALTITUDE FAIL",
                                 AlertSeverity::warning, false});
    }
    if (!s.navigation.cdi_deviation_dots.usable() ||
        !s.attitude.heading_deg.usable()) {
        result.active.push_back({"NAV_DATA_FAIL", "NAV DATA FAIL",
                                 AlertSeverity::caution, false});
    }
    if (s.health.stale_fields > 0) {
        result.active.push_back({"DATA_STALE", "DATA STALE",
                                 AlertSeverity::caution, false});
    }
    // Un dato fermo non e' scaduto e non e' assente: ogni altro controllo lo
    // considera buono. Se non venisse detto qui, non verrebbe detto da nessuna
    // parte, e lo strumento continuerebbe a mostrare un numero convincente.
    //
    // L'allarme nomina la grandezza. Una spia che dice soltanto "un dato e'
    // fermo" costringe il pilota a cercare quale, nel momento in cui ha meno
    // tempo per farlo.
    if (s.health.suspect_fields > 0) {
        std::string message = "DATA FROZEN";
        if (s.health.frozen_signal != SignalId::unknown) {
            message = std::string{signal_name(s.health.frozen_signal)} + " FROZEN";
        }
        result.active.push_back({"DATA_FROZEN", message,
                                 AlertSeverity::caution, false});
    }

    // Due catene indipendenti che non concordano. Nessuna delle due viene
    // presentata come buona, perche' il confronto non dice quale menta.
    if (s.health.disagreeing_fields > 0) {
        result.active.push_back({"DATA_DISAGREE", "SPEED DISAGREE",
                                 AlertSeverity::warning, false});
    }

    if (is_simulated(s.attitude.roll_deg) ||
        is_simulated(s.air_data.indicated_airspeed_kt) ||
        is_simulated(s.air_data.pressure_altitude_ft)) {
        result.active.push_back({"SIMULATION", "SIMULATION ACTIVE",
                                 AlertSeverity::advisory, false});
    }

    std::stable_sort(result.active.begin(), result.active.end(),
                     [](const Alert& left, const Alert& right) {
                         return priority(left.severity) > priority(right.severity);
                     });
    return result;
}

}  // namespace efis
