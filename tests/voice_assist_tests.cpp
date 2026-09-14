#include "efis/voice/voice_assist.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;

void require(bool condition, const std::string& message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void commands_are_ptt_gated_and_exact() {
    efis::voice::VoiceCommandGate gate{{true, true, 0.80}};
    auto decision = gate.evaluate({efis::voice::Language::italian, "Confermo!", 0.96,
                                   false, true, true});
    require(!decision.accepted && decision.state == efis::voice::RecognitionState::standby,
            "a command without PTT must remain standby");

    decision = gate.evaluate({efis::voice::Language::italian, "  CONFERMO. ", 0.96,
                              true, true, true});
    require(decision.accepted && decision.intent == efis::voice::VoiceIntent::acknowledge,
            "an exact normalized Italian command must be accepted");
    require(!decision.transmission_or_actuation_allowed,
            "an accepted voice intent must never authorize transmission or actuation");

    decision = gate.evaluate({efis::voice::Language::english, "turn the radio on", 0.99,
                              true, true, false});
    require(!decision.accepted &&
            decision.reason == "OUTSIDE_APPROVED_GRAMMAR",
            "free-form or actuator-like speech must be rejected");
}

void confidence_health_and_enablement_fail_closed() {
    efis::voice::VoiceCommandGate gate{{false, true, 0.85}};
    auto decision = gate.evaluate({efis::voice::Language::english, "confirm", 0.99,
                                   true, true, false});
    require(decision.state == efis::voice::RecognitionState::disabled,
            "voice must start disabled when configured so");
    gate.set_enabled(true);
    decision = gate.evaluate({efis::voice::Language::english, "confirm", 0.84,
                              true, true, false});
    require(decision.state == efis::voice::RecognitionState::uncertain && !decision.accepted,
            "confidence below the configured threshold must not produce an intent");
    decision = gate.evaluate({efis::voice::Language::english, "confirm", 0.99,
                              true, false, false});
    require(decision.state == efis::voice::RecognitionState::unavailable && !decision.accepted,
            "an unhealthy recognizer must fail closed");
}

void acknowledgements_do_not_claim_sensor_verification() {
    efis::voice::VoiceCommandGate gate{{true, true, 0.80}};
    efis::voice::AcknowledgementTracker tracker{"checklist-001"};
    const auto decision = gate.evaluate({efis::voice::Language::italian, "fatto", 0.94,
                                         true, true, true});
    require(!tracker.apply_voice_decision(decision),
            "an item that was not presented must not be acknowledged");
    tracker.mark_presented();
    require(tracker.apply_voice_decision(decision),
            "a presented item may record a recognized pilot acknowledgement");
    require(tracker.record().pilot_acknowledged && tracker.record().acknowledged_by_voice,
            "voice provenance must be retained");
    require(tracker.record().sensor_verification == efis::voice::SensorVerification::pending,
            "pilot acknowledgement must not imply sensor verification");
}

efis::voice::PhraseologyTemplate simulation_template() {
    return {
        "sim.initial-contact.it",
        efis::voice::Language::italian,
        "IT",
        "SIM-1",
        "https://example.invalid/simulation-only",
        "sha256:simulation",
        "{STATION}, {CALLSIGN}, {POSITION}, quota {ALTITUDE}, richiesta {REQUEST}",
        {"STATION", "CALLSIGN", "POSITION", "ALTITUDE", "REQUEST"},
        {"ALTITUDE"},
        false,
        true,
    };
}

void phraseology_requires_complete_context_and_remains_advisory() {
    efis::voice::PhraseologyEngine engine;
    auto phrase = simulation_template();
    auto output = engine.compose(phrase, {{"STATION", "Treviso"}});
    require(output.state == efis::voice::PhraseologyState::blocked &&
            !output.missing_fields.empty() && output.rendered_text.empty(),
            "missing context must block phrase composition");

    const efis::voice::PhraseologyContext context{
        {"STATION", "Treviso Torre"}, {"CALLSIGN", "I-TEST"},
        {"POSITION", "punto Sierra"}, {"ALTITUDE", "duemila piedi"},
        {"REQUEST", "ingresso nel circuito"},
    };
    output = engine.compose(phrase, context);
    require(output.state == efis::voice::PhraseologyState::simulation_draft &&
            output.rendered_text.find("I-TEST") != std::string::npos,
            "a complete simulation package may produce only a simulation draft");
    require(output.pilot_review_required && !output.transmit_allowed && output.simulated,
            "even a complete phrase must require pilot review and prohibit transmission");

    phrase.simulated = false;
    phrase.approved = false;
    output = engine.compose(phrase, context);
    require(output.state == efis::voice::PhraseologyState::blocked &&
            output.reason == "PACKAGE_NOT_APPROVED",
            "a real unapproved package must fail closed");
}

void malformed_templates_are_rejected() {
    efis::voice::PhraseologyEngine engine;
    auto phrase = simulation_template();
    phrase.text += " {UNKNOWN}";
    const efis::voice::PhraseologyContext context{
        {"STATION", "A"}, {"CALLSIGN", "B"}, {"POSITION", "C"},
        {"ALTITUDE", "D"}, {"REQUEST", "E"},
    };
    const auto output = engine.compose(phrase, context);
    require(output.state == efis::voice::PhraseologyState::blocked &&
            output.reason == "UNRESOLVED_TEMPLATE_TOKEN",
            "unresolved placeholders must never leak into a displayed phrase");
}
}  // namespace

int main() {
    commands_are_ptt_gated_and_exact();
    confidence_health_and_enablement_fail_closed();
    acknowledgements_do_not_claim_sensor_verification();
    phraseology_requires_complete_context_and_remains_advisory();
    malformed_templates_are_rejected();
    if (failures == 0) std::cout << "Voice Assist tests: PASS\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
