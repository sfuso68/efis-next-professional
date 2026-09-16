#include "efis/emergency/emergency_audio.hpp"
#include "efis/emergency/emergency_input.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

namespace {
int failures = 0;

void require(bool condition, const std::string& message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void physical_input_is_debounced_held_and_rearmed() {
    efis::emergency::EmergencyInputMonitor monitor{{50ms, 1200ms, 5000ms, 300ms}};
    const auto now = efis::MonoClock::now();
    auto value = monitor.update({false, true}, now);
    require(!value.armed, "input must require a stable released interval before arming");
    value = monitor.update({false, true}, now + 300ms);
    require(value.armed && value.health == efis::emergency::EmergencyInputHealth::ready,
            "healthy released input must arm");

    (void)monitor.update({true, true}, now + 310ms);
    value = monitor.update({false, true}, now + 340ms);
    require(!value.activation_edge, "contact bounce must not activate Emergency Assist");
    (void)monitor.update({true, true}, now + 400ms);
    value = monitor.update({true, true}, now + 449ms);
    require(!value.stable_pressed, "press shorter than debounce must remain unstable");
    value = monitor.update({true, true}, now + 450ms);
    require(value.stable_pressed && !value.activation_edge,
            "debounced press must still wait for long-hold threshold");
    value = monitor.update({true, true}, now + 1650ms);
    require(value.activation_edge && !value.armed,
            "one activation edge must be generated after the configured hold");
    value = monitor.update({true, true}, now + 2000ms);
    require(!value.activation_edge, "held input must not generate repeated activations");

    (void)monitor.update({false, true}, now + 2100ms);
    (void)monitor.update({false, true}, now + 2150ms);
    value = monitor.update({false, true}, now + 2450ms);
    require(value.armed, "stable release must rearm the input");
}

void unhealthy_and_stuck_inputs_fail_closed() {
    efis::emergency::EmergencyInputMonitor monitor{{10ms, 100ms, 500ms, 20ms}};
    const auto now = efis::MonoClock::now();
    auto value = monitor.update({true, false}, now);
    require(!value.armed &&
            value.health == efis::emergency::EmergencyInputHealth::source_unhealthy,
            "unhealthy adapter must be unable to activate");
    (void)monitor.update({false, true}, now + 10ms);
    (void)monitor.update({false, true}, now + 20ms);
    (void)monitor.update({false, true}, now + 40ms);
    (void)monitor.update({true, true}, now + 50ms);
    (void)monitor.update({true, true}, now + 60ms);
    value = monitor.update({true, true}, now + 560ms);
    require(value.health == efis::emergency::EmergencyInputHealth::stuck_pressed &&
            !value.armed,
            "stuck input must be diagnosed and disarmed");
}

void invalid_input_timing_is_rejected() {
    bool rejected = false;
    try {
        efis::emergency::EmergencyInputMonitor monitor{{10ms, 500ms, 100ms, 10ms}};
        (void)monitor;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "stuck threshold shorter than hold must be rejected");
}

void audio_priority_preempts_and_preserves_critical_cues() {
    efis::emergency::EmergencyAudioArbiter arbiter;
    const auto now = efis::MonoClock::now();
    require(arbiter.enqueue({"checklist", "CHECK ITEM", efis::emergency::AudioCuePriority::advisory, 1s}, now),
            "first advisory must be accepted");
    require(arbiter.enqueue({"terrain", "TERRAIN", efis::emergency::AudioCuePriority::critical, 1s}, now + 1ms),
            "critical cue must be accepted");
    require(arbiter.output().active && arbiter.output().active->id == "terrain" &&
            arbiter.output().interrupted,
            "critical cue must preempt an advisory");
    arbiter.complete_active(now + 2ms);
    require(arbiter.output().active && arbiter.output().active->id == "checklist",
            "interrupted advisory may resume after critical cue completion");

    arbiter.set_noncritical_muted(true);
    require(!arbiter.output().active,
            "mute policy must remove a noncritical active cue");
    require(!arbiter.enqueue({"status", "STATUS", efis::emergency::AudioCuePriority::caution, 1s}, now + 3ms),
            "mute policy must reject advisory/caution cues");
    require(arbiter.enqueue({"stall", "STALL", efis::emergency::AudioCuePriority::warning, 1s}, now + 4ms),
            "warning cue must bypass noncritical mute");
}

void audio_repeat_inhibit_is_deterministic() {
    efis::emergency::EmergencyAudioArbiter arbiter;
    const auto now = efis::MonoClock::now();
    const efis::emergency::AudioCue cue{"emergency", "EMERGENCY", efis::emergency::AudioCuePriority::warning, 2s};
    require(arbiter.enqueue(cue, now), "cue must enqueue initially");
    require(!arbiter.enqueue(cue, now + 1ms), "duplicate active cue must be rejected");
    arbiter.complete_active(now + 100ms);
    require(!arbiter.enqueue(cue, now + 1s), "cue inside repeat inhibit must be rejected");
    require(arbiter.enqueue(cue, now + 2100ms), "cue after repeat inhibit must be accepted");
}
}  // namespace

int main() {
    physical_input_is_debounced_held_and_rearmed();
    unhealthy_and_stuck_inputs_fail_closed();
    invalid_input_timing_is_rejected();
    audio_priority_preempts_and_preserves_critical_cues();
    audio_repeat_inhibit_is_deterministic();
    if (failures == 0) std::cout << "Emergency I/O tests: PASS\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
