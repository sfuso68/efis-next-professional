#include "efis/providers/sensor_adapter_health.hpp"

namespace efis {

bool SensorAdapterHealth::configure(SensorChannelConfig config,
                                    SensorChannelPolicy policy) noexcept {
    const bool identity_valid = config.signal != SignalId::unknown &&
                                config.node != node_undeclared &&
                                config.channel != channel_undeclared &&
                                config.unit != Unit::unknown &&
                                config.calibration_revision != 0;
    const bool timing_valid = policy.stale_after.count() > 0 &&
                              policy.lost_after > policy.stale_after &&
                              policy.recovery_samples > 0;
    if (!identity_valid || !timing_valid) {
        configured_ = false;
        config_ = {};
        policy_ = {};
        status_ = {};
        return false;
    }

    config_ = config;
    policy_ = policy;
    status_ = {};
    configured_ = true;
    return true;
}

SensorChannelState SensorAdapterHealth::nominal_state() const noexcept {
    return config_.mode == SensorAdapterMode::simulation
               ? SensorChannelState::simulated
               : SensorChannelState::valid;
}

SensorObservationVerdict SensorAdapterHealth::observe(
    MonoTime at, std::uint64_t sequence, bool simulated, SensorFault fault) noexcept {
    if (!configured_) return SensorObservationVerdict::rejected_unconfigured;

    const bool expected_simulated = config_.mode == SensorAdapterMode::simulation;
    if (simulated != expected_simulated) {
        ++status_.mode_mismatches;
        return SensorObservationVerdict::rejected_mode_mismatch;
    }

    if (status_.last_observation.has_value() && at < *status_.last_observation) {
        ++status_.time_regressions;
        return SensorObservationVerdict::rejected_regressed_time;
    }

    if (status_.last_sequence.has_value()) {
        if (sequence == *status_.last_sequence) {
            ++status_.duplicates;
            return SensorObservationVerdict::rejected_duplicate;
        }
        if (sequence < *status_.last_sequence) {
            ++status_.regressions;
            return SensorObservationVerdict::rejected_regressed_sequence;
        }
    }

    std::uint64_t gap = 0;
    if (status_.last_sequence.has_value()) {
        gap = sequence - *status_.last_sequence - 1;
    }
    status_.last_sequence = sequence;

    if (fault != SensorFault::none) {
        status_.last_fault = fault;
        ++status_.fault_events;
        status_.recovery_progress = 0;
        status_.state = SensorChannelState::failed;
        return SensorObservationVerdict::fault_reported;
    }

    const bool recovering = status_.state == SensorChannelState::failed ||
                            status_.state == SensorChannelState::lost ||
                            status_.state == SensorChannelState::stale;
    status_.last_observation = at;
    status_.last_observation_simulated = simulated;
    ++status_.accepted;
    status_.missed += gap;

    if (recovering && status_.recovery_progress + 1 < policy_.recovery_samples) {
        ++status_.recovery_progress;
        return SensorObservationVerdict::accepted_recovery_pending;
    }

    status_.recovery_progress = policy_.recovery_samples;
    status_.last_fault = SensorFault::none;
    status_.state = nominal_state();
    return gap > 0 ? SensorObservationVerdict::accepted_after_gap
                   : SensorObservationVerdict::accepted;
}

SensorChannelState SensorAdapterHealth::update(MonoTime now) noexcept {
    if (!configured_ || !status_.last_observation.has_value()) return status_.state;
    if (status_.state == SensorChannelState::failed) return status_.state;
    if (now < *status_.last_observation) {
        ++status_.time_regressions;
        status_.state = SensorChannelState::failed;
        status_.last_fault = SensorFault::clock_regression;
        ++status_.fault_events;
        status_.recovery_progress = 0;
        return status_.state;
    }

    const auto age = now - *status_.last_observation;
    if (age >= policy_.lost_after) {
        status_.state = SensorChannelState::lost;
        status_.recovery_progress = 0;
    } else if (age >= policy_.stale_after) {
        status_.state = SensorChannelState::stale;
        status_.recovery_progress = 0;
    }
    return status_.state;
}

void SensorAdapterHealth::restart(MonoTime at) noexcept {
    (void)at;
    const auto restarts = status_.restarts + 1;
    status_ = {};
    status_.last_fault = SensorFault::adapter_restart;
    status_.restarts = restarts;
}

}  // namespace efis
