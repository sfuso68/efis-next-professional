#include "efis/emergency/emergency_input.hpp"

#include <algorithm>
#include <stdexcept>

namespace efis::emergency {

EmergencyInputMonitor::EmergencyInputMonitor(EmergencyInputConfig config) : config_(config) {
    if (config_.debounce.count() < 0 || config_.hold.count() <= 0 ||
        config_.stuck < config_.hold || config_.release_rearm.count() < 0) {
        throw std::invalid_argument("invalid emergency input timing");
    }
}

const EmergencyInputOutput& EmergencyInputMonitor::update(
    EmergencyInputSample sample, MonoTime now) {
    output_.activation_edge = false;

    if (!initialized_) {
        initialized_ = true;
        raw_pressed_ = sample.pressed;
        stable_pressed_ = sample.pressed;
        raw_changed_at_ = now;
        stable_changed_at_ = now;
    }

    if (!sample.source_healthy) {
        armed_ = false;
        activation_sent_ = false;
        raw_pressed_ = sample.pressed;
        stable_pressed_ = sample.pressed;
        raw_changed_at_ = now;
        stable_changed_at_ = now;
        output_.health = EmergencyInputHealth::source_unhealthy;
        output_.armed = false;
        output_.stable_pressed = stable_pressed_;
        output_.pressed_for = std::chrono::milliseconds{0};
        output_.diagnostic = "SOURCE UNHEALTHY";
        return output_;
    }

    if (sample.pressed != raw_pressed_) {
        raw_pressed_ = sample.pressed;
        raw_changed_at_ = now;
    }

    if (raw_pressed_ != stable_pressed_ && now - raw_changed_at_ >= config_.debounce) {
        stable_pressed_ = raw_pressed_;
        stable_changed_at_ = now;
        activation_sent_ = false;
    }

    if (!stable_pressed_) {
        const auto released_for =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - stable_changed_at_);
        if (released_for >= config_.release_rearm) armed_ = true;
        output_.health = armed_ ? EmergencyInputHealth::ready : EmergencyInputHealth::rearming;
        output_.pressed_for = std::chrono::milliseconds{0};
        output_.diagnostic = armed_ ? "READY" : "REARMING - RELEASE INPUT";
    } else {
        const auto pressed_for =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - stable_changed_at_);
        output_.pressed_for = std::max(pressed_for, std::chrono::milliseconds{0});
        if (pressed_for >= config_.stuck) {
            armed_ = false;
            output_.health = EmergencyInputHealth::stuck_pressed;
            output_.diagnostic = "INPUT STUCK PRESSED";
        } else if (armed_ && !activation_sent_ && pressed_for >= config_.hold) {
            output_.activation_edge = true;
            activation_sent_ = true;
            armed_ = false;
            output_.health = EmergencyInputHealth::rearming;
            output_.diagnostic = "ACTIVATED - RELEASE TO REARM";
        } else {
            output_.health = EmergencyInputHealth::rearming;
            output_.diagnostic = armed_ ? "HOLD IN PROGRESS" : "RELEASE TO REARM";
        }
    }

    output_.armed = armed_;
    output_.stable_pressed = stable_pressed_;
    return output_;
}

const char* input_health_name(EmergencyInputHealth health) noexcept {
    switch (health) {
        case EmergencyInputHealth::ready: return "READY";
        case EmergencyInputHealth::source_unhealthy: return "SOURCE_UNHEALTHY";
        case EmergencyInputHealth::stuck_pressed: return "STUCK_PRESSED";
        case EmergencyInputHealth::rearming: return "REARMING";
    }
    return "UNKNOWN";
}

}  // namespace efis::emergency
