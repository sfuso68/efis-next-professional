#include "efis/core/snapshot_arbitrator.hpp"

#include <algorithm>

namespace efis {
namespace {

// Tabella delle grandezze arbitrate.
//
// Ogni voce lega un segnale al campo che lo contiene, in lettura e in
// scrittura. Tenerla come dato invece che ripetere diciotto volte lo stesso
// blocco di codice ha un vantaggio concreto: aggiungere una grandezza e'
// aggiungere una riga, e l'elenco di cio' che viene arbitrato si legge tutto
// insieme, senza doverlo ricostruire scorrendo una funzione lunga.
template <class T>
struct FieldAccess final {
    SignalId signal;
    const Sample<T>& (*read)(const FlightSnapshot&);
    Sample<T>& (*write)(FlightSnapshot&);
};

constexpr FieldAccess<double> numeric_fields[] = {
    {SignalId::roll,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.attitude.roll_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.attitude.roll_deg; }},
    {SignalId::pitch,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.attitude.pitch_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.attitude.pitch_deg; }},
    {SignalId::heading,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.attitude.heading_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.attitude.heading_deg; }},
    {SignalId::turn_rate,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.attitude.turn_rate_deg_s; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.attitude.turn_rate_deg_s; }},
    {SignalId::lateral_acceleration,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.attitude.lateral_acceleration_g; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.attitude.lateral_acceleration_g; }},

    {SignalId::indicated_airspeed,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.air_data.indicated_airspeed_kt; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.air_data.indicated_airspeed_kt; }},
    {SignalId::pressure_altitude,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.air_data.pressure_altitude_ft; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.air_data.pressure_altitude_ft; }},
    {SignalId::vertical_speed,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.air_data.vertical_speed_fpm; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.air_data.vertical_speed_fpm; }},

    {SignalId::latitude,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.position.latitude_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.position.latitude_deg; }},
    {SignalId::longitude,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.position.longitude_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.position.longitude_deg; }},
    {SignalId::ground_speed,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.position.ground_speed_kt; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.position.ground_speed_kt; }},
    {SignalId::gnss_vertical_speed,
     [](const FlightSnapshot& s) -> const Sample<double>& {
         return s.position.gnss_vertical_speed_fpm;
     },
     [](FlightSnapshot& s) -> Sample<double>& {
         return s.position.gnss_vertical_speed_fpm;
     }},

    {SignalId::commanded_roll,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.guidance.commanded_roll_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.guidance.commanded_roll_deg; }},
    {SignalId::commanded_pitch,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.guidance.commanded_pitch_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.guidance.commanded_pitch_deg; }},
    {SignalId::selected_altitude,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.guidance.selected_altitude_ft; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.guidance.selected_altitude_ft; }},
    {SignalId::selected_course,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.guidance.selected_course_deg; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.guidance.selected_course_deg; }},
    {SignalId::selected_airspeed,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.guidance.selected_airspeed_kt; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.guidance.selected_airspeed_kt; }},

    {SignalId::cdi_deviation,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.navigation.cdi_deviation_dots; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.navigation.cdi_deviation_dots; }},
    {SignalId::glide_slope_deviation,
     [](const FlightSnapshot& s) -> const Sample<double>& { return s.navigation.glide_slope_dots; },
     [](FlightSnapshot& s) -> Sample<double>& { return s.navigation.glide_slope_dots; }},
};

constexpr FieldAccess<bool> discrete_fields[] = {
    {SignalId::flight_director_active,
     [](const FlightSnapshot& s) -> const Sample<bool>& { return s.guidance.flight_director_active; },
     [](FlightSnapshot& s) -> Sample<bool>& { return s.guidance.flight_director_active; }},
    {SignalId::to_flag,
     [](const FlightSnapshot& s) -> const Sample<bool>& { return s.navigation.to_flag; },
     [](FlightSnapshot& s) -> Sample<bool>& { return s.navigation.to_flag; }},
};

constexpr std::size_t numeric_count = std::size(numeric_fields);
constexpr std::size_t discrete_count = std::size(discrete_fields);

}  // namespace

SnapshotArbitrator::SnapshotArbitrator(ArbitrationPolicy policy) : policy_(policy) {
    numeric_.reserve(numeric_count);
    for (std::size_t i = 0; i < numeric_count; ++i) numeric_.emplace_back(policy_);
    discrete_.reserve(discrete_count);
    for (std::size_t i = 0; i < discrete_count; ++i) discrete_.emplace_back(policy_);
}

FlightSnapshot SnapshotArbitrator::combine(
    std::span<const SnapshotContribution> contributions, MonoTime now) {
    FlightSnapshot out{};

    auto record = [&](const std::optional<SourceChangeEvent>& change) {
        if (!change.has_value()) return;
        events_.push_back(*change);
        switch (change->reason) {
            case SourceChangeReason::failover: ++statistics_.failovers; break;
            case SourceChangeReason::reversion: ++statistics_.reversions; break;
            case SourceChangeReason::initial: break;
        }
    };

    // Un campione utilizzabile che dichiara un segnale diverso da quello del
    // campo in cui si trova e' un difetto di chi lo produce. Viene contato, non
    // corretto: riscrivere l'etichetta renderebbe la registrazione una
    // descrizione falsa di cio' che e' successo.
    auto check_label = [&](SignalId expected, SignalId declared, bool usable) {
        if (usable && declared != SignalId::unknown && declared != expected) {
            ++statistics_.mislabelled_samples;
        }
    };

    std::vector<Sample<double>> numeric_candidates;
    numeric_candidates.reserve(contributions.size());
    for (std::size_t field = 0; field < numeric_count; ++field) {
        const auto& access = numeric_fields[field];
        numeric_candidates.clear();
        for (const SnapshotContribution& contribution : contributions) {
            const Sample<double>& candidate = access.read(contribution.snapshot);
            check_label(access.signal, candidate.origin.signal, candidate.usable());
            numeric_candidates.push_back(candidate);
        }
        auto result = numeric_[field].select(numeric_candidates, now);
        access.write(out) = result.selected;
        record(result.change);
    }

    std::vector<Sample<bool>> discrete_candidates;
    discrete_candidates.reserve(contributions.size());
    for (std::size_t field = 0; field < discrete_count; ++field) {
        const auto& access = discrete_fields[field];
        discrete_candidates.clear();
        for (const SnapshotContribution& contribution : contributions) {
            const Sample<bool>& candidate = access.read(contribution.snapshot);
            check_label(access.signal, candidate.origin.signal, candidate.usable());
            discrete_candidates.push_back(candidate);
        }
        auto result = discrete_[field].select(discrete_candidates, now);
        access.write(out) = result.selected;
        record(result.change);
    }

    // Salute e allarmi non si arbitrano: dipendono dal risultato dell'arbitrato
    // e li calcola FlightModel::normalize, che viene subito dopo. Calcolarli qui
    // significherebbe farlo due volte, con il rischio di due risposte diverse.
    out.timestamp = now;
    out.sequence = ++sequence_;
    return out;
}

void SnapshotArbitrator::reset() noexcept {
    for (auto& arbitrator : numeric_) arbitrator = SourceArbitrator<double>{policy_};
    for (auto& arbitrator : discrete_) arbitrator = SourceArbitrator<bool>{policy_};
}

SourceId SnapshotArbitrator::selected_for(SignalId signal) const noexcept {
    for (std::size_t field = 0; field < numeric_count; ++field) {
        if (numeric_fields[field].signal == signal) {
            return numeric_[field].selected_source();
        }
    }
    for (std::size_t field = 0; field < discrete_count; ++field) {
        if (discrete_fields[field].signal == signal) {
            return discrete_[field].selected_source();
        }
    }
    return SourceId::unknown;
}

}  // namespace efis
