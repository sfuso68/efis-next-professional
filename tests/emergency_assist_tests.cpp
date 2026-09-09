#include "efis/emergency/emergency_assist.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace std::chrono_literals;

namespace {

int failures = 0;

void require(bool condition, const std::string& message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

template <class T>
efis::Sample<T> valid(T value, efis::SignalId signal, bool simulated = false) {
    efis::Sample<T> sample{};
    sample.value = value;
    sample.validity = simulated ? efis::Validity::simulated : efis::Validity::valid;
    sample.quality = efis::Quality::nominal;
    sample.origin.signal = signal;
    sample.origin.simulated = simulated;
    return sample;
}

efis::emergency::AssistInput complete_input(bool simulated = false) {
    efis::emergency::AssistInput input{};
    input.flight.attitude.roll_deg = valid(0.0, efis::SignalId::roll, simulated);
    input.flight.attitude.pitch_deg = valid(0.0, efis::SignalId::pitch, simulated);
    input.flight.air_data.indicated_airspeed_kt =
        valid(70.0, efis::SignalId::indicated_airspeed, simulated);
    input.flight.air_data.pressure_altitude_ft =
        valid(5000.0, efis::SignalId::pressure_altitude, simulated);
    input.flight.air_data.vertical_speed_fpm =
        valid(-500.0, efis::SignalId::vertical_speed, simulated);
    input.flight.position.latitude_deg = valid(46.0, efis::SignalId::latitude, simulated);
    input.flight.position.longitude_deg = valid(12.0, efis::SignalId::longitude, simulated);
    input.engine.rpm = valid(5000.0, efis::SignalId::engine_rpm, simulated);
    input.engine.manifold_pressure_inhg =
        valid(25.0, efis::SignalId::manifold_pressure, simulated);
    input.environment = {3500.0, true, true, true, true, true, true, true, true,
                         true, true, 0.9};
    return input;
}

void manual_activation_is_advisory_and_fail_closed() {
    efis::emergency::EmergencyAssist assist;
    auto input = complete_input(true);
    const auto now = efis::MonoClock::now();
    assist.activate_manual(efis::emergency::EmergencyKind::engine_power_loss,
                           "touch-long-press", now);
    require(assist.output().state == efis::emergency::AssistState::pilot_activated,
            "manual activation must stop for pilot confirmation");
    assist.confirm(efis::emergency::EmergencyKind::engine_power_loss, now + 1ms);
    const auto& output = assist.update(input, now + 2ms);
    require(output.state == efis::emergency::AssistState::data_degraded,
            "confirmed event with missing approvals must enter degraded FLY state");
    require(!output.target_speed_kt.has_value(),
            "unapproved profile must never expose a numeric best-glide target");
    require(!output.glide_estimate_available,
            "unapproved performance must block glide range");
    require(!output.procedure_available,
            "missing approved procedure must block checklist content");
    require(output.simulation_only, "simulated inputs must remain visible in the output");
}

void missing_flight_data_degrades_without_inventing_values() {
    efis::emergency::EmergencyAssist assist;
    efis::emergency::AssistInput input{};
    const auto now = efis::MonoClock::now();
    assist.activate_manual(efis::emergency::EmergencyKind::sensor_or_pfd_loss,
                           "physical-input", now);
    assist.confirm(efis::emergency::EmergencyKind::sensor_or_pfd_loss, now + 1ms);
    const auto& output = assist.update(input, now + 2ms);
    require(output.state == efis::emergency::AssistState::data_insufficient,
            "invalid primary data must enter DATA_INSUFFICIENT");
    require(!output.glide_estimate_available && output.ranked_airports.empty(),
            "missing position/terrain must not produce landing reachability");
    require(!output.degradation_reasons.empty(),
            "every unavailable dependency must be explained");
}

void simulated_glide_is_never_labelled_authoritative() {
    efis::emergency::EmergencyAssist assist;
    auto input = complete_input(true);
    input.performance = {true, true, 68.0, 8.0, 500.0, 250.0, 250.0, 500.0};
    input.airports = {
        {"LIDB", "Belluno", 2.0, 10.0, 1200.0, 800.0, 20.0, 5.0, true, true, true},
        {"BAD", "Blocked", 1.0, 20.0, 1000.0, 1000.0, 25.0, 0.0, true, false, true}};
    const auto now = efis::MonoClock::now();
    assist.activate_manual(efis::emergency::EmergencyKind::engine_power_loss, "test", now);
    assist.confirm(efis::emergency::EmergencyKind::engine_power_loss, now + 1ms);
    (void)assist.update(input, now + 2ms);
    assist.acknowledge_fly(now + 3ms);
    const auto& output = assist.update(input, now + 4ms);
    require(output.glide_estimate_available, "approved SIM model may support simulator geometry");
    require(!output.authoritative_reachability,
            "SIM performance or inputs must never yield authoritative reachability");
    require(!output.ranked_airports.empty() &&
            output.ranked_airports.front().assessment ==
                efis::emergency::CandidateAssessment::reachable_simulation,
            "reachable simulator candidate must be labelled SIM_ESTIMATE");
    require(output.ranked_airports.size() == 2 &&
            output.ranked_airports.back().assessment ==
                efis::emergency::CandidateAssessment::excluded,
            "terrain-blocked candidate must remain excluded");
}

void approved_inputs_can_rank_but_only_after_all_gates() {
    efis::emergency::EmergencyAssist assist;
    auto input = complete_input(false);
    input.performance = {true, false, 67.0, 9.0, 400.0, 200.0, 300.0, 400.0};
    input.airports = {
        {"A", "Alpha", 2.0, 5.0, 100.0, 900.0, 20.0, 8.0, true, true, true},
        {"B", "Bravo", 20.0, 15.0, 100.0, 1500.0, 30.0, 0.0, true, true, true},
        {"C", "Charlie", 1.0, 25.0, 100.0, 500.0, 15.0, 0.0, false, true, true}};
    const auto now = efis::MonoClock::now();
    assist.activate_manual(efis::emergency::EmergencyKind::engine_power_loss, "test", now);
    assist.confirm(efis::emergency::EmergencyKind::engine_power_loss, now + 1ms);
    const auto& output = assist.update(input, now + 2ms);
    require(output.target_speed_kt == 67.0, "approved best glide may be exposed");
    require(output.authoritative_reachability,
            "only fully approved real inputs may pass authoritative gate");
    require(output.ranked_airports.front().airport.identifier == "A" &&
            output.ranked_airports.front().assessment ==
                efis::emergency::CandidateAssessment::reachable_approved,
            "eligible runway must rank before excluded candidates");
}

void procedure_requires_complete_approval_metadata() {
    efis::emergency::EmergencyAssist assist;
    auto input = complete_input(false);
    input.procedure = {"PWR-LOSS", "1", "POH p.3", "abc123", true,
                       {{"S1", "FLY", "APPROVED TEST STEP", true},
                        {"S2", "MANAGE", "SECOND APPROVED TEST STEP", true}}};
    const auto now = efis::MonoClock::now();
    assist.activate_manual(efis::emergency::EmergencyKind::engine_power_loss, "test", now);
    assist.confirm(efis::emergency::EmergencyKind::engine_power_loss, now + 1ms);
    (void)assist.update(input, now + 2ms);
    assist.acknowledge_fly(now + 3ms);
    const auto& output = assist.update(input, now + 4ms);
    require(output.state == efis::emergency::AssistState::checklist_active,
            "approved package may open checklist after FLY acknowledgement");
    require(output.current_step && output.current_step->id == "S1",
            "first approved step must be exposed deterministically");
    assist.skip_step(now + 5ms);
    require(assist.events().back().code == "PROCEDURE_STEP_SKIPPED",
            "skip must be recorded as a distinct event");
    const auto& second = assist.update(input, now + 6ms);
    require(second.current_step && second.current_step->id == "S2",
            "skip must advance exactly one step");

    input.procedure.content_hash.clear();
    const auto& rejected = assist.update(input, now + 7ms);
    require(!rejected.procedure_available && !rejected.current_step,
            "missing provenance/hash must fail closed even if approved flag is true");
}

void automatic_detection_needs_approval_two_signals_and_dwell() {
    efis::emergency::DetectionConfig detection{true, true, true, 3000.0, 0.5, 0.6, 500ms};
    efis::emergency::EmergencyAssist assist{detection};
    auto input = complete_input(true);
    const auto now = efis::MonoClock::now();
    (void)assist.update(input, now);
    input.engine.rpm.value = 2000.0;
    (void)assist.update(input, now + 100ms);
    require(assist.output().state == efis::emergency::AssistState::idle,
            "RPM alone must not trigger suspicion");
    input.engine.manifold_pressure_inhg.value = 10.0;
    (void)assist.update(input, now + 200ms);
    require(assist.output().state == efis::emergency::AssistState::idle,
            "two-signal condition must persist for configured dwell");
    (void)assist.update(input, now + 701ms);
    require(assist.output().state == efis::emergency::AssistState::suspected,
            "approved persistent multi-signal condition may only raise suspicion");
    require(assist.output().kind == efis::emergency::EmergencyKind::engine_power_loss,
            "detector must propose, not silently execute, the scenario");
}

void unapproved_detector_cannot_trigger() {
    efis::emergency::DetectionConfig detection{true, false, true, 3000.0, 0.9, 0.9, 1ms};
    efis::emergency::EmergencyAssist assist{detection};
    auto input = complete_input(true);
    const auto now = efis::MonoClock::now();
    (void)assist.update(input, now);
    input.engine.rpm.value = 100.0;
    input.engine.manifold_pressure_inhg.value = 1.0;
    (void)assist.update(input, now + 1s);
    require(assist.output().state == efis::emergency::AssistState::idle,
            "unapproved detection thresholds must remain inert");
}

void all_advisory_families_require_pilot_confirmation() {
    using Kind = efis::emergency::EmergencyKind;
    const Kind kinds[]{Kind::engine_power_loss, Kind::abnormal_attitude,
        Kind::unsafe_airspeed, Kind::electrical_failure, Kind::smoke_or_fire,
        Kind::weather_or_terrain, Kind::sensor_or_pfd_loss,
        Kind::pilot_incapacitation};
    auto now = efis::MonoClock::now();
    for (const auto kind : kinds) {
        efis::emergency::EmergencyAssist assist;
        assist.activate_manual(kind, "taxonomy-test", now);
        require(assist.output().state == efis::emergency::AssistState::pilot_activated,
                std::string{"family bypassed pilot confirmation: "} +
                    efis::emergency::kind_name(kind));
        require(std::string{efis::emergency::kind_name(kind)} != "SELECT EMERGENCY",
                "every selectable family must have a stable display name");
        now += 1ms;
    }
}

void pilot_can_change_emergency_without_restarting_the_session() {
    efis::emergency::EmergencyAssist assist;
    auto input = complete_input(false);
    const auto now = efis::MonoClock::now();
    assist.activate_manual(efis::emergency::EmergencyKind::engine_power_loss,
                           "test", now);
    assist.confirm(efis::emergency::EmergencyKind::engine_power_loss, now + 1ms);
    (void)assist.update(input, now + 2ms);
    assist.confirm(efis::emergency::EmergencyKind::electrical_failure, now + 3ms);
    require(assist.output().state == efis::emergency::AssistState::pilot_confirmed &&
            assist.output().kind == efis::emergency::EmergencyKind::electrical_failure,
            "pilot must be able to correct the selected emergency");
    require(assist.events().back().code == "EMERGENCY_CHANGED",
            "emergency correction must be auditable");
}

}  // namespace

int main() {
    manual_activation_is_advisory_and_fail_closed();
    missing_flight_data_degrades_without_inventing_values();
    simulated_glide_is_never_labelled_authoritative();
    approved_inputs_can_rank_but_only_after_all_gates();
    procedure_requires_complete_approval_metadata();
    automatic_detection_needs_approval_two_signals_and_dwell();
    unapproved_detector_cannot_trigger();
    all_advisory_families_require_pilot_confirmation();
    pilot_can_change_emergency_without_restarting_the_session();
    if (failures == 0) std::cout << "Emergency Assist tests: PASS\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
