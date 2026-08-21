#pragma once

#include "efis/core/sample.hpp"

#include <string>
#include <vector>

namespace efis {

struct AttitudeState final {
    Sample<double> roll_deg;
    Sample<double> pitch_deg;
    Sample<double> heading_deg;
};

struct AirDataState final {
    Sample<double> indicated_airspeed_kt;
    Sample<double> pressure_altitude_ft;
    Sample<double> vertical_speed_fpm;
};

struct PositionState final {
    Sample<double> latitude_deg;
    Sample<double> longitude_deg;
    Sample<double> ground_speed_kt;

    // Velocita' verticale ricavata dal satellitare. Sta qui e non fra i dati
    // aria perche' e' cio' che la distingue: non passa dalla presa statica, e
    // quindi non si guasta insieme a quella barometrica.
    Sample<double> gnss_vertical_speed_fpm;
};

struct GuidanceState final {
    Sample<bool> flight_director_active;
    Sample<double> commanded_roll_deg;
    Sample<double> commanded_pitch_deg;
    Sample<double> selected_altitude_ft;
    Sample<double> selected_course_deg;
    Sample<double> selected_airspeed_kt;
};

struct NavigationState final {
    Sample<double> cdi_deviation_dots;
    Sample<double> glide_slope_dots;
    Sample<bool> to_flag;
};

enum class AlertSeverity : std::uint8_t { advisory, caution, warning };

struct Alert final {
    std::string code;
    std::string message;
    AlertSeverity severity{AlertSeverity::advisory};
    bool acknowledged{};
};

struct AlertState final {
    std::vector<Alert> active;
};

enum class HealthLevel : std::uint8_t { normal, advisory, caution, warning };

struct SystemHealth final {
    HealthLevel level{HealthLevel::warning};
    std::uint32_t unavailable_fields{};
    std::uint32_t stale_fields{};

    // Campi che continuano ad arrivare freschi ma il cui valore non cambia piu'.
    // Non sono scaduti: sono fermi, che e' una condizione diversa e piu'
    // insidiosa, perche' ogni altro controllo li considera buoni. Vedi ADR-004.
    std::uint32_t suspect_fields{};

    // Campi marcati in disaccordo dal confronto fra catene indipendenti.
    std::uint32_t disagreeing_fields{};

    // Campi che l'autopilota, o un'altra sorgente, dichiara di qualita' ridotta.
    // Continuano a essere mostrati: degradato non e' assente, ed e' proprio la
    // distinzione che permette di non togliere al pilota un valore che potrebbe
    // essergli ancora utile.
    std::uint32_t degraded_fields{};

    // Grandezza ferma da segnalare per prima. Una spia che dice soltanto "un
    // dato e' fermo" costringe il pilota a cercare quale, nel momento in cui ha
    // meno tempo per farlo.
    SignalId frozen_signal{SignalId::unknown};
};

struct FlightSnapshot final {
    AttitudeState attitude;
    AirDataState air_data;
    PositionState position;
    GuidanceState guidance;
    NavigationState navigation;
    AlertState alerts;
    SystemHealth health;
    MonoTime timestamp{};
    std::uint64_t sequence{};
};

}  // namespace efis
