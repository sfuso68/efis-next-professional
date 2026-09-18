#include "efis/providers/sensor_adapter_health.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

efis::SensorChannelConfig real_config() {
    return {.signal = efis::SignalId::oil_pressure,
            .node = 51,
            .channel = 3,
            .unit = efis::Unit::pound_per_square_inch,
            .mode = efis::SensorAdapterMode::real,
            .calibration_revision = 7};
}

efis::SensorChannelPolicy policy() {
    return {.stale_after = 250ms, .lost_after = 1000ms, .recovery_samples = 3};
}

efis::SensorAdapterHealth configured_real() {
    efis::SensorAdapterHealth health;
    require(health.configure(real_config(), policy()), "valid real configuration rejected");
    return health;
}

void point01_rejects_incomplete_identity() {
    efis::SensorAdapterHealth health;
    auto config = real_config();
    config.channel = efis::channel_undeclared;
    require(!health.configure(config, policy()), "undeclared channel accepted");
    require(!health.configured(), "invalid identity must leave adapter unconfigured");
}

void point02_rejects_incoherent_timing() {
    efis::SensorAdapterHealth health;
    auto invalid = policy();
    invalid.lost_after = invalid.stale_after;
    require(!health.configure(real_config(), invalid), "lost <= stale policy accepted");
    invalid = policy();
    invalid.recovery_samples = 0;
    require(!health.configure(real_config(), invalid), "zero recovery dwell accepted");
}

void point03_starts_unknown_and_unusable() {
    const auto health = configured_real();
    require(health.status().state == efis::SensorChannelState::unknown,
            "configured channel must start UNKNOWN");
    require(!efis::sensor_channel_usable(health.status().state),
            "UNKNOWN channel must be unusable");
}

void point04_accepts_first_real_observation() {
    auto health = configured_real();
    require(health.observe(efis::MonoTime{} + 10ms, 1, false) ==
                efis::SensorObservationVerdict::accepted,
            "first real observation rejected");
    require(health.status().state == efis::SensorChannelState::valid,
            "real observation must become VALID");
}

void point05_keeps_simulation_structural() {
    auto config = real_config();
    config.mode = efis::SensorAdapterMode::simulation;
    efis::SensorAdapterHealth health;
    require(health.configure(config, policy()), "simulation configuration rejected");
    require(health.observe(efis::MonoTime{} + 10ms, 1, true) ==
                efis::SensorObservationVerdict::accepted,
            "simulation observation rejected");
    require(health.status().state == efis::SensorChannelState::simulated &&
                health.status().last_observation_simulated,
            "simulation provenance was lost");
}

void point06_rejects_mode_mismatch() {
    auto health = configured_real();
    require(health.observe(efis::MonoTime{} + 10ms, 1, true) ==
                efis::SensorObservationVerdict::rejected_mode_mismatch,
            "SIM value accepted by real channel");
    require(health.status().mode_mismatches == 1 && health.status().accepted == 0,
            "mode mismatch counters are wrong");
}

void point07_rejects_duplicate_sequence_without_freshening() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 4, false);
    require(health.observe(first + 50ms, 4, false) ==
                efis::SensorObservationVerdict::rejected_duplicate,
            "duplicate sequence accepted");
    require(health.status().last_observation == first && health.status().duplicates == 1,
            "duplicate incorrectly refreshed the channel");
}

void point08_rejects_regressed_sequence() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 8, false);
    require(health.observe(first + 10ms, 7, false) ==
                efis::SensorObservationVerdict::rejected_regressed_sequence,
            "regressed sequence accepted");
    require(health.status().regressions == 1 && health.status().last_sequence == 8,
            "regressed sequence altered accepted state");
}

void point09_records_sequence_gaps() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 2, false);
    require(health.observe(first + 10ms, 6, false) ==
                efis::SensorObservationVerdict::accepted_after_gap,
            "sequence gap not reported");
    require(health.status().missed == 3, "missing sample count is wrong");
}

void point10_rejects_observation_time_regression() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 100ms;
    (void)health.observe(first, 1, false);
    require(health.observe(first - 1ms, 2, false) ==
                efis::SensorObservationVerdict::rejected_regressed_time,
            "regressed observation time accepted");
    require(health.status().time_regressions == 1 && health.status().last_sequence == 1,
            "time regression altered accepted sequence");
}

void verify_fault(efis::SensorFault fault, const char* message) {
    auto health = configured_real();
    const auto verdict = health.observe(efis::MonoTime{} + 10ms, 1, false, fault);
    require(verdict == efis::SensorObservationVerdict::fault_reported, message);
    require(health.status().state == efis::SensorChannelState::failed &&
                health.status().last_fault == fault &&
                health.status().fault_events == 1 &&
                !health.status().last_observation.has_value(),
            "fault must fail closed without creating a good observation");
}

void point11_board_absent_fails_closed() {
    verify_fault(efis::SensorFault::board_absent, "board absence was ignored");
}

void point12_bus_timeout_fails_closed() {
    verify_fault(efis::SensorFault::bus_timeout, "bus timeout was ignored");
}

void point13_open_input_fails_closed() {
    verify_fault(efis::SensorFault::open_input, "open input was ignored");
}

void point14_short_input_fails_closed() {
    verify_fault(efis::SensorFault::short_input, "short input was ignored");
}

void point15_invalid_range_fails_closed() {
    verify_fault(efis::SensorFault::invalid_range, "invalid range was ignored");
}

void point16_transitions_to_stale_then_lost() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 1, false);
    require(health.update(first + 249ms) == efis::SensorChannelState::valid,
            "channel became stale too early");
    require(health.update(first + 250ms) == efis::SensorChannelState::stale,
            "channel did not become stale at threshold");
    require(health.update(first + 1000ms) == efis::SensorChannelState::lost,
            "channel did not become lost at threshold");
}

void point17_failed_state_does_not_self_clear_with_time() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 1, false, efis::SensorFault::short_input);
    require(health.update(first + 10s) == efis::SensorChannelState::failed,
            "fault cleared without valid recovery samples");
}

void point18_requires_recovery_dwell() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 1, false);
    (void)health.observe(first + 1ms, 2, false, efis::SensorFault::open_input);
    require(health.observe(first + 2ms, 3, false) ==
                efis::SensorObservationVerdict::accepted_recovery_pending,
            "first recovery sample armed channel too early");
    require(health.observe(first + 3ms, 4, false) ==
                efis::SensorObservationVerdict::accepted_recovery_pending,
            "second recovery sample armed channel too early");
    require(health.observe(first + 4ms, 5, false) ==
                efis::SensorObservationVerdict::accepted,
            "third recovery sample did not rearm channel");
    require(health.status().state == efis::SensorChannelState::valid &&
                health.status().last_fault == efis::SensorFault::none,
            "successful dwell did not restore nominal state");
}

void point19_restart_discards_freshness_and_sequence() {
    auto health = configured_real();
    const auto first = efis::MonoTime{} + 10ms;
    (void)health.observe(first, 50, false);
    health.restart(first + 1ms);
    require(health.status().state == efis::SensorChannelState::unknown &&
                !health.status().last_observation.has_value() &&
                !health.status().last_sequence.has_value() &&
                health.status().last_fault == efis::SensorFault::adapter_restart &&
                health.status().restarts == 1,
            "restart retained live data or sequence");
    require(health.observe(first + 2ms, 1, false) ==
                efis::SensorObservationVerdict::accepted,
            "new publisher session after restart was rejected");
}

void point20_maps_only_valid_and_simulated_to_usable() {
    require(efis::sensor_channel_validity(efis::SensorChannelState::valid) ==
                efis::Validity::valid,
            "VALID mapping wrong");
    require(efis::sensor_channel_validity(efis::SensorChannelState::simulated) ==
                efis::Validity::simulated,
            "SIMULATED mapping wrong");
    require(efis::sensor_channel_validity(efis::SensorChannelState::stale) ==
                efis::Validity::stale,
            "STALE mapping wrong");
    for (const auto state : {efis::SensorChannelState::unknown,
                             efis::SensorChannelState::stale,
                             efis::SensorChannelState::lost,
                             efis::SensorChannelState::failed}) {
        require(!efis::sensor_channel_usable(state), "unsafe state reported usable");
    }
    const auto health = configured_real();
    require(health.config().signal == efis::SignalId::oil_pressure &&
                health.config().calibration_revision == 7,
            "channel identity or calibration revision changed");
}

}  // namespace

int main() {
    point01_rejects_incomplete_identity();
    point02_rejects_incoherent_timing();
    point03_starts_unknown_and_unusable();
    point04_accepts_first_real_observation();
    point05_keeps_simulation_structural();
    point06_rejects_mode_mismatch();
    point07_rejects_duplicate_sequence_without_freshening();
    point08_rejects_regressed_sequence();
    point09_records_sequence_gaps();
    point10_rejects_observation_time_regression();
    point11_board_absent_fails_closed();
    point12_bus_timeout_fails_closed();
    point13_open_input_fails_closed();
    point14_short_input_fails_closed();
    point15_invalid_range_fails_closed();
    point16_transitions_to_stale_then_lost();
    point17_failed_state_does_not_self_clear_with_time();
    point18_requires_recovery_dwell();
    point19_restart_discards_freshness_and_sequence();
    point20_maps_only_valid_and_simulated_to_usable();
    std::cout << "sensor adapter health tests passed (20/20)\n";
    return 0;
}
