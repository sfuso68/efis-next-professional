#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/engine/engine_snapshot.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace efis::emergency {

enum class EmergencyKind : std::uint8_t {
    none,
    engine_power_loss,
    abnormal_attitude,
    unsafe_airspeed,
    electrical_failure,
    smoke_or_fire,
    weather_or_terrain,
    sensor_or_pfd_loss,
    pilot_incapacitation
};

enum class AssistState : std::uint8_t {
    idle,
    suspected,
    pilot_activated,
    pilot_confirmed,
    fly_priority,
    data_insufficient,
    landing_options,
    checklist_active,
    immediate_landing,
    no_reachable_airport,
    power_recovered,
    monitoring,
    data_degraded,
    dismissed,
    cancelled
};

enum class CandidateAssessment : std::uint8_t {
    not_assessed,
    excluded,
    reachable_simulation,
    reachable_approved
};

struct DetectionConfig final {
    bool enabled{};
    bool approved{};
    bool allow_simulation{};
    double minimum_baseline_rpm{};
    double rpm_ratio_threshold{};
    double independent_ratio_threshold{};
    std::chrono::milliseconds dwell{};
};

struct PerformanceModel final {
    bool approved{};
    bool simulated{};
    std::optional<double> best_glide_kt;
    std::optional<double> conservative_glide_ratio;
    double terrain_clearance_reserve_ft{};
    double manoeuvre_loss_ft{};
    double model_uncertainty_ft{};
    double final_approach_reserve_ft{};
};

struct EnvironmentState final {
    std::optional<double> agl_ft;
    bool agl_valid{};
    bool position_valid{};
    bool terrain_valid{};
    bool obstacles_valid{};
    bool airport_database_valid{};
    bool wind_valid{};
    bool weather_valid{};
    bool blackbox_link_valid{};
    bool in_flight_known{};
    bool in_flight{};
    double wind_range_factor{1.0};
};

struct AirportCandidate final {
    std::string identifier;
    std::string name;
    double distance_nm{};
    double bearing_deg{};
    double runway_elevation_ft{};
    double runway_length_m{};
    double runway_width_m{};
    double headwind_kt{};
    bool runway_compatible{};
    bool route_clear{};
    bool data_current{};
};

struct ProcedureStep final {
    std::string id;
    std::string phase;
    std::string text;
    bool confirmation_required{true};
};

struct ProcedurePackage final {
    std::string package_id;
    std::string revision;
    std::string source_reference;
    std::string content_hash;
    bool approved{};
    std::vector<ProcedureStep> steps;
};

struct AssistInput final {
    FlightSnapshot flight;
    engine::EngineSnapshot engine;
    EnvironmentState environment;
    PerformanceModel performance;
    std::vector<AirportCandidate> airports;
    ProcedurePackage procedure;
};

struct RankedAirport final {
    AirportCandidate airport;
    CandidateAssessment assessment{CandidateAssessment::not_assessed};
    double estimated_arrival_margin_ft{};
    double score{};
    std::string reason;
};

struct AssistEvent final {
    std::uint64_t sequence{};
    MonoTime at{};
    std::string code;
    std::string detail;
};

struct AssistOutput final {
    AssistState state{AssistState::idle};
    EmergencyKind kind{EmergencyKind::none};
    std::string headline;
    std::string fly_advisory;
    std::string navigation_advisory;
    std::string communication_draft;
    std::string voice_cue;
    bool procedure_available{};
    bool glide_estimate_available{};
    bool authoritative_reachability{};
    bool simulation_only{};
    std::optional<double> target_speed_kt;
    std::optional<double> conservative_range_nm;
    std::vector<std::string> degradation_reasons;
    std::vector<RankedAirport> ranked_airports;
    std::optional<ProcedureStep> current_step;
    std::size_t completed_steps{};
};

class EmergencyAssist final {
public:
    explicit EmergencyAssist(DetectionConfig detection = {});

    [[nodiscard]] const AssistOutput& update(const AssistInput& input, MonoTime now);
    void activate_manual(EmergencyKind proposed, std::string trigger, MonoTime now);
    void confirm(EmergencyKind selected, MonoTime now);
    void dismiss_suspicion(MonoTime now);
    void acknowledge_fly(MonoTime now);
    void complete_step(MonoTime now);
    void skip_step(MonoTime now);
    void report_power_recovered(MonoTime now);
    void continue_monitoring(MonoTime now);
    void cancel(std::string reason, MonoTime now);
    void reset(MonoTime now);
    void repeat_cue(MonoTime now);

    [[nodiscard]] const AssistOutput& output() const noexcept { return output_; }
    [[nodiscard]] const std::vector<AssistEvent>& events() const noexcept { return events_; }
    [[nodiscard]] std::size_t event_cursor() const noexcept { return events_.size(); }

private:
    void transition(AssistState state, std::string code, std::string detail, MonoTime now);
    void evaluate_detection(const AssistInput& input, MonoTime now);
    void rebuild_output(const AssistInput& input, MonoTime now);
    void evaluate_glide(const AssistInput& input);
    void record(std::string code, std::string detail, MonoTime now);

    DetectionConfig detection_{};
    AssistOutput output_{};
    std::vector<AssistEvent> events_;
    std::uint64_t next_event_sequence_{1};
    std::optional<MonoTime> detection_started_;
    double baseline_rpm_{};
    double baseline_independent_{};
    std::size_t procedure_index_{};
};

[[nodiscard]] const char* kind_name(EmergencyKind kind) noexcept;
[[nodiscard]] const char* state_name(AssistState state) noexcept;
[[nodiscard]] const char* assessment_name(CandidateAssessment assessment) noexcept;

}  // namespace efis::emergency
