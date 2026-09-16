#include "efis/emergency/emergency_assist.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace efis::emergency {
namespace {

constexpr double feet_per_nautical_mile = 6076.12;

bool finite_positive(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool primary_flight_data_available(const FlightSnapshot& flight) {
    return flight.attitude.roll_deg.usable() && flight.attitude.pitch_deg.usable() &&
           flight.air_data.indicated_airspeed_kt.usable() &&
           flight.air_data.pressure_altitude_ft.usable() &&
           flight.air_data.vertical_speed_fpm.usable();
}

bool primary_flight_data_simulated(const FlightSnapshot& flight) {
    return flight.attitude.roll_deg.is_simulated() || flight.attitude.pitch_deg.is_simulated() ||
           flight.air_data.indicated_airspeed_kt.is_simulated() ||
           flight.air_data.pressure_altitude_ft.is_simulated();
}

struct AdvisoryProfile {
    const char* fly;
    const char* navigation;
    const char* communication;
    const char* voice;
};

// These are deliberately non-procedural prompts. Aircraft actions and numeric
// targets are supplied only by an approved, provenance-checked procedure or
// performance package; this table never substitutes for the POH/AFM.
constexpr AdvisoryProfile advisory_profile(EmergencyKind kind) noexcept {
    switch (kind) {
        case EmergencyKind::engine_power_loss:
            return {"MAINTAIN CONTROL - USE APPROVED POWER-LOSS PROCEDURE",
                    "ASSESS LANDING OPTIONS ONLY WHEN ALL DATA GATES PASS",
                    "MAYDAY DRAFT - ENGINE EVENT; VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
                    "Engine emergency. Maintain control. Verify data and approved procedure."};
        case EmergencyKind::abnormal_attitude:
            return {"MAINTAIN CONTROL - USE RELIABLE SOURCES AND APPROVED RECOVERY PROCEDURE",
                    "NAVIGATION DEFERRED UNTIL ATTITUDE IS STABILISED",
                    "MAYDAY DRAFT - CONTROL EVENT; VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
                    "Attitude emergency. Maintain control. Cross-check reliable sources."};
        case EmergencyKind::unsafe_airspeed:
            return {"MAINTAIN CONTROL - VERIFY IAS VALIDITY AND APPROVED ENVELOPE",
                    "NAVIGATION DEFERRED UNTIL AIRSPEED IS STABILISED",
                    "MAYDAY DRAFT - AIRSPEED EVENT; VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
                    "Airspeed emergency. Maintain control. Cross-check airspeed sources."};
        case EmergencyKind::electrical_failure:
            return {"MAINTAIN CONTROL - USE APPROVED ELECTRICAL FAILURE PROCEDURE",
                    "ASSESS NAVIGATION AND DISPLAY AVAILABILITY",
                    "PAN OR MAYDAY DRAFT - ELECTRICAL EVENT; VERIFY CALLSIGN, POSITION AND INTENTIONS",
                    "Electrical emergency. Maintain control. Verify remaining systems."};
        case EmergencyKind::smoke_or_fire:
            return {"MAINTAIN CONTROL - USE APPROVED SMOKE OR FIRE PROCEDURE",
                    "ASSESS LANDING OPTIONS; VERIFY TERRAIN AND WEATHER DATA",
                    "MAYDAY DRAFT - SMOKE OR FIRE; VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
                    "Smoke or fire. Maintain control. Use the approved procedure."};
        case EmergencyKind::weather_or_terrain:
            return {"MAINTAIN A SAFE FLIGHT PATH - VERIFY TERRAIN AND WEATHER DATA",
                    "REVIEW ESCAPE OPTIONS ONLY FROM CURRENT VALIDATED DATA",
                    "PAN OR MAYDAY DRAFT - WEATHER OR TERRAIN; VERIFY POSITION, ALTITUDE AND INTENTIONS",
                    "Weather or terrain hazard. Maintain a safe flight path. Verify data."};
        case EmergencyKind::sensor_or_pfd_loss:
            return {"MAINTAIN CONTROL - CROSS-CHECK INDEPENDENT INSTRUMENTS",
                    "USE ONLY NAVIGATION SOURCES THAT REMAIN VALID",
                    "PAN DRAFT - INSTRUMENT FAILURE; VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
                    "Instrument failure. Maintain control. Cross-check independent sources."};
        case EmergencyKind::pilot_incapacitation:
            return {"MAINTAIN CONTROL - REQUEST ASSISTANCE AND USE APPROVED PROCEDURE",
                    "ASSESS THE SIMPLEST SUITABLE LANDING OPTION FROM VALID DATA",
                    "MAYDAY DRAFT - PILOT INCAPACITATION; VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
                    "Pilot incapacitation. Maintain control. Request assistance."};
        case EmergencyKind::none:
            break;
    }
    return {"FLY - USE THE APPROVED AIRCRAFT PROCEDURE",
            "NAVIGATION OPTIONS NOT ASSESSED",
            "EMERGENCY DRAFT - VERIFY CALLSIGN, POSITION, ALTITUDE AND INTENTIONS",
            "Emergency assist ready."};
}

}  // namespace

EmergencyAssist::EmergencyAssist(DetectionConfig detection) : detection_(detection) {}

const AssistOutput& EmergencyAssist::update(const AssistInput& input, MonoTime now) {
    evaluate_detection(input, now);
    rebuild_output(input, now);
    return output_;
}

void EmergencyAssist::activate_manual(EmergencyKind proposed, std::string trigger, MonoTime now) {
    output_.kind = proposed;
    procedure_index_ = 0;
    transition(AssistState::pilot_activated, "MANUAL_ACTIVATION", std::move(trigger), now);
}

void EmergencyAssist::confirm(EmergencyKind selected, MonoTime now) {
    if (selected == EmergencyKind::none) return;
    if (output_.state != AssistState::suspected &&
        output_.state != AssistState::pilot_activated) {
        if (output_.state == AssistState::idle || output_.state == AssistState::dismissed ||
            output_.state == AssistState::cancelled) return;
        output_.kind = selected;
        procedure_index_ = 0;
        transition(AssistState::pilot_confirmed, "EMERGENCY_CHANGED",
                   kind_name(selected), now);
        return;
    }
    output_.kind = selected;
    procedure_index_ = 0;
    transition(AssistState::pilot_confirmed, "PILOT_CONFIRMED", kind_name(selected), now);
}

void EmergencyAssist::dismiss_suspicion(MonoTime now) {
    if (output_.state != AssistState::suspected) return;
    transition(AssistState::dismissed, "SUSPICION_DISMISSED", "pilot", now);
}

void EmergencyAssist::acknowledge_fly(MonoTime now) {
    if (output_.state != AssistState::fly_priority &&
        output_.state != AssistState::data_degraded &&
        output_.state != AssistState::data_insufficient) return;
    transition(AssistState::landing_options, "FLY_ACKNOWLEDGED", "pilot", now);
}

void EmergencyAssist::complete_step(MonoTime now) {
    if (output_.state != AssistState::checklist_active || !output_.current_step) return;
    record("PROCEDURE_STEP_COMPLETED", output_.current_step->id, now);
    ++procedure_index_;
}

void EmergencyAssist::skip_step(MonoTime now) {
    if (output_.state != AssistState::checklist_active || !output_.current_step) return;
    record("PROCEDURE_STEP_SKIPPED", output_.current_step->id, now);
    ++procedure_index_;
}

void EmergencyAssist::report_power_recovered(MonoTime now) {
    if (output_.kind != EmergencyKind::engine_power_loss ||
        output_.state == AssistState::idle || output_.state == AssistState::cancelled) return;
    transition(AssistState::power_recovered, "POWER_RECOVERY_REPORTED", "pilot", now);
}

void EmergencyAssist::continue_monitoring(MonoTime now) {
    if (output_.state != AssistState::power_recovered) return;
    transition(AssistState::monitoring, "MONITORING_SELECTED", "pilot", now);
}

void EmergencyAssist::cancel(std::string reason, MonoTime now) {
    if (output_.state == AssistState::idle) return;
    transition(AssistState::cancelled, "ASSIST_CANCELLED", std::move(reason), now);
}

void EmergencyAssist::reset(MonoTime now) {
    output_ = {};
    detection_started_.reset();
    baseline_rpm_ = 0.0;
    baseline_independent_ = 0.0;
    procedure_index_ = 0;
    record("ASSIST_RESET", "idle", now);
}

void EmergencyAssist::repeat_cue(MonoTime now) {
    if (output_.state == AssistState::idle) return;
    record("VOICE_CUE_REPEAT", output_.voice_cue, now);
}

void EmergencyAssist::transition(AssistState state, std::string code,
                                 std::string detail, MonoTime now) {
    if (output_.state == state) return;
    output_.state = state;
    record(std::move(code), std::move(detail), now);
}

void EmergencyAssist::evaluate_detection(const AssistInput& input, MonoTime now) {
    if (output_.state != AssistState::idle && output_.state != AssistState::dismissed) return;
    if (!detection_.enabled || !detection_.approved || !input.environment.in_flight_known ||
        !input.environment.in_flight || !input.engine.rpm.usable() ||
        (!detection_.allow_simulation && input.engine.rpm.is_simulated()) ||
        !finite_positive(detection_.minimum_baseline_rpm) ||
        !(detection_.rpm_ratio_threshold > 0.0 && detection_.rpm_ratio_threshold < 1.0) ||
        !(detection_.independent_ratio_threshold > 0.0 &&
          detection_.independent_ratio_threshold < 1.0) || detection_.dwell.count() <= 0) {
        detection_started_.reset();
        return;
    }

    const auto& independent = input.engine.manifold_pressure_inhg.usable()
        ? input.engine.manifold_pressure_inhg : input.engine.fuel_flow_gph;
    if (!independent.usable() || (!detection_.allow_simulation && independent.is_simulated())) {
        detection_started_.reset();
        return;
    }

    baseline_rpm_ = std::max(baseline_rpm_, input.engine.rpm.value);
    baseline_independent_ = std::max(baseline_independent_, independent.value);
    if (baseline_rpm_ < detection_.minimum_baseline_rpm || !finite_positive(baseline_independent_)) {
        detection_started_.reset();
        return;
    }

    const bool rpm_drop = input.engine.rpm.value / baseline_rpm_ <= detection_.rpm_ratio_threshold;
    const bool corroborated = independent.value / baseline_independent_ <=
                              detection_.independent_ratio_threshold;
    if (!rpm_drop || !corroborated) {
        detection_started_.reset();
        if (output_.state == AssistState::dismissed) output_.state = AssistState::idle;
        return;
    }
    // Dopo un rigetto del pilota la stessa condizione non deve ripresentarsi
    // in loop. Il detector si riabilita soltanto dopo che almeno uno dei due
    // segnali e' tornato fuori dalla condizione di caduta.
    if (output_.state == AssistState::dismissed) return;
    if (!detection_started_) detection_started_ = now;
    if (now - *detection_started_ < detection_.dwell) return;

    output_.kind = EmergencyKind::engine_power_loss;
    transition(AssistState::suspected, "POWER_LOSS_SUSPECTED",
               "multi-signal detection; pilot confirmation required", now);
}

void EmergencyAssist::rebuild_output(const AssistInput& input, MonoTime now) {
    const auto profile = advisory_profile(output_.kind);
    output_.headline = output_.state == AssistState::suspected
        ? "ENGINE POWER? - CONFIRM"
        : output_.state == AssistState::idle ? "EMERGENCY ASSIST READY"
                                             : kind_name(output_.kind);
    output_.fly_advisory = profile.fly;
    output_.navigation_advisory = profile.navigation;
    output_.communication_draft = profile.communication;
    output_.voice_cue = output_.state == AssistState::suspected
        ? "Engine power. Confirm."
        : profile.voice;
    output_.procedure_available = input.procedure.approved &&
        !input.procedure.package_id.empty() && !input.procedure.revision.empty() &&
        !input.procedure.source_reference.empty() && !input.procedure.content_hash.empty() &&
        !input.procedure.steps.empty();
    output_.simulation_only = input.performance.simulated ||
                              primary_flight_data_simulated(input.flight);
    output_.target_speed_kt.reset();
    output_.conservative_range_nm.reset();
    output_.glide_estimate_available = false;
    output_.authoritative_reachability = false;
    output_.degradation_reasons.clear();
    output_.ranked_airports.clear();
    output_.current_step.reset();
    output_.completed_steps = procedure_index_;

    if (!primary_flight_data_available(input.flight))
        output_.degradation_reasons.emplace_back("PRIMARY FLIGHT DATA INCOMPLETE");
    if (!input.environment.agl_valid || !input.environment.agl_ft)
        output_.degradation_reasons.emplace_back("AGL UNAVAILABLE");
    if (!input.environment.position_valid)
        output_.degradation_reasons.emplace_back("POSITION UNAVAILABLE");
    if (!input.environment.terrain_valid)
        output_.degradation_reasons.emplace_back("TERRAIN DATA UNAVAILABLE");
    if (!input.environment.obstacles_valid)
        output_.degradation_reasons.emplace_back("OBSTACLE DATA UNAVAILABLE");
    if (!input.environment.wind_valid)
        output_.degradation_reasons.emplace_back("WIND UNKNOWN");
    if (!input.environment.weather_valid)
        output_.degradation_reasons.emplace_back("WEATHER UNKNOWN OR STALE");
    if (!input.environment.blackbox_link_valid)
        output_.degradation_reasons.emplace_back("BLACKBOX EVENT SINK UNAVAILABLE");
    if (!input.performance.approved)
        output_.degradation_reasons.emplace_back("AIRCRAFT PERFORMANCE NOT APPROVED");
    if (!output_.procedure_available)
        output_.degradation_reasons.emplace_back("APPROVED PROCEDURE UNAVAILABLE");

    if (input.performance.approved && input.performance.best_glide_kt &&
        finite_positive(*input.performance.best_glide_kt)) {
        output_.target_speed_kt = input.performance.best_glide_kt;
    }

    evaluate_glide(input);

    if (output_.state == AssistState::pilot_confirmed) {
        const auto next = !primary_flight_data_available(input.flight)
            ? AssistState::data_insufficient
            : !output_.degradation_reasons.empty() ? AssistState::data_degraded
                                                   : AssistState::fly_priority;
        transition(next,
                   "FLY_PRIORITY_ENTERED", "validated input gate", now);
    }
    if (output_.state == AssistState::landing_options) {
        if (output_.ranked_airports.empty()) {
            output_.navigation_advisory = output_.glide_estimate_available
                ? "NO AIRPORT PASSED ALL GATES" : "REACHABILITY NOT AVAILABLE";
        } else {
            output_.navigation_advisory = "REVIEW RANKED LANDING OPTIONS";
        }
        if (output_.procedure_available) {
            transition(AssistState::checklist_active, "CHECKLIST_OPENED",
                       input.procedure.package_id + "@" + input.procedure.revision, now);
        }
    }
    if (output_.state == AssistState::checklist_active && output_.procedure_available &&
        procedure_index_ < input.procedure.steps.size()) {
        output_.current_step = input.procedure.steps[procedure_index_];
    }
    if (output_.state == AssistState::checklist_active && output_.procedure_available &&
        procedure_index_ >= input.procedure.steps.size()) {
        output_.headline = "APPROVED CHECKLIST STEPS REVIEWED";
    }
}

void EmergencyAssist::evaluate_glide(const AssistInput& input) {
    const auto& env = input.environment;
    const auto& model = input.performance;
    if (!model.approved || !model.conservative_glide_ratio ||
        !finite_positive(*model.conservative_glide_ratio) || !env.agl_valid || !env.agl_ft ||
        !env.position_valid || !env.terrain_valid || !env.obstacles_valid ||
        !env.airport_database_valid || !env.wind_valid) return;

    const double reserve = model.terrain_clearance_reserve_ft + model.manoeuvre_loss_ft +
                           model.model_uncertainty_ft + model.final_approach_reserve_ft;
    const double usable_height = std::max(0.0, *env.agl_ft - reserve);
    const double wind_factor = std::clamp(env.wind_range_factor, 0.25, 1.0);
    const double range_nm = usable_height * *model.conservative_glide_ratio /
                            feet_per_nautical_mile * wind_factor;
    output_.conservative_range_nm = range_nm;
    output_.glide_estimate_available = true;
    output_.authoritative_reachability = !model.simulated &&
        !primary_flight_data_simulated(input.flight);

    for (const auto& airport : input.airports) {
        RankedAirport ranked{};
        ranked.airport = airport;
        if (!airport.data_current) {
            ranked.assessment = CandidateAssessment::excluded;
            ranked.reason = "AIRPORT DATA STALE";
        } else if (!airport.runway_compatible) {
            ranked.assessment = CandidateAssessment::excluded;
            ranked.reason = "RUNWAY NOT APPROVED FOR PROFILE";
        } else if (!airport.route_clear) {
            ranked.assessment = CandidateAssessment::excluded;
            ranked.reason = "TERRAIN/OBSTACLE ROUTE NOT CLEAR";
        } else if (airport.distance_nm > range_nm) {
            ranked.assessment = CandidateAssessment::excluded;
            ranked.reason = "OUTSIDE CONSERVATIVE RANGE";
        } else {
            ranked.estimated_arrival_margin_ft =
                (range_nm - airport.distance_nm) * feet_per_nautical_mile /
                *model.conservative_glide_ratio;
            ranked.score = ranked.estimated_arrival_margin_ft +
                           std::min(airport.runway_length_m, 2000.0) * 0.1 +
                           std::clamp(airport.headwind_kt, -30.0, 30.0) * 10.0;
            ranked.assessment = output_.authoritative_reachability
                ? CandidateAssessment::reachable_approved
                : CandidateAssessment::reachable_simulation;
            ranked.reason = output_.authoritative_reachability
                ? "PASSED APPROVED CONSERVATIVE GATES"
                : "SIMULATION ESTIMATE ONLY";
        }
        output_.ranked_airports.push_back(std::move(ranked));
    }
    std::stable_sort(output_.ranked_airports.begin(), output_.ranked_airports.end(),
        [](const RankedAirport& left, const RankedAirport& right) {
            const bool left_reachable = left.assessment == CandidateAssessment::reachable_approved ||
                                        left.assessment == CandidateAssessment::reachable_simulation;
            const bool right_reachable = right.assessment == CandidateAssessment::reachable_approved ||
                                         right.assessment == CandidateAssessment::reachable_simulation;
            if (left_reachable != right_reachable) return left_reachable;
            return left.score > right.score;
        });
}

void EmergencyAssist::record(std::string code, std::string detail, MonoTime now) {
    events_.push_back({next_event_sequence_++, now, std::move(code), std::move(detail)});
}

const char* kind_name(EmergencyKind kind) noexcept {
    switch (kind) {
        case EmergencyKind::none: return "SELECT EMERGENCY";
        case EmergencyKind::engine_power_loss: return "ENGINE POWER LOSS";
        case EmergencyKind::abnormal_attitude: return "ABNORMAL ATTITUDE";
        case EmergencyKind::unsafe_airspeed: return "UNSAFE AIRSPEED";
        case EmergencyKind::electrical_failure: return "ELECTRICAL FAILURE";
        case EmergencyKind::smoke_or_fire: return "SMOKE / FIRE";
        case EmergencyKind::weather_or_terrain: return "WEATHER / TERRAIN";
        case EmergencyKind::sensor_or_pfd_loss: return "SENSOR / PFD LOSS";
        case EmergencyKind::pilot_incapacitation: return "PILOT INCAPACITATION";
    }
    return "SELECT EMERGENCY";
}

const char* state_name(AssistState state) noexcept {
    switch (state) {
        case AssistState::idle: return "IDLE";
        case AssistState::suspected: return "SUSPECTED";
        case AssistState::pilot_activated: return "PILOT_ACTIVATED";
        case AssistState::pilot_confirmed: return "PILOT_CONFIRMED";
        case AssistState::fly_priority: return "FLY_PRIORITY";
        case AssistState::data_insufficient: return "DATA_INSUFFICIENT";
        case AssistState::landing_options: return "LANDING_OPTIONS";
        case AssistState::checklist_active: return "CHECKLIST_ACTIVE";
        case AssistState::immediate_landing: return "IMMEDIATE_LANDING";
        case AssistState::no_reachable_airport: return "NO_REACHABLE_AIRPORT";
        case AssistState::power_recovered: return "POWER_RECOVERED";
        case AssistState::monitoring: return "MONITORING";
        case AssistState::data_degraded: return "DATA_DEGRADED";
        case AssistState::dismissed: return "DISMISSED";
        case AssistState::cancelled: return "CANCELLED";
    }
    return "IDLE";
}

const char* assessment_name(CandidateAssessment assessment) noexcept {
    switch (assessment) {
        case CandidateAssessment::not_assessed: return "NOT_ASSESSED";
        case CandidateAssessment::excluded: return "EXCLUDED";
        case CandidateAssessment::reachable_simulation: return "SIM_ESTIMATE";
        case CandidateAssessment::reachable_approved: return "REACHABLE";
    }
    return "NOT_ASSESSED";
}

}  // namespace efis::emergency
