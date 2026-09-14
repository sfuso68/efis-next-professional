#include "efis/voice/voice_assist.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace efis::voice {
namespace {

using Grammar = std::unordered_map<std::string, VoiceIntent>;

const Grammar& italian_grammar() {
    static const Grammar grammar{
        {"confermo", VoiceIntent::acknowledge},
        {"fatto", VoiceIntent::complete},
        {"ripeti", VoiceIntent::repeat},
        {"passo successivo", VoiceIntent::next_item},
        {"salta", VoiceIntent::skip},
        {"non possibile", VoiceIntent::unable},
        {"annulla", VoiceIntent::cancel},
        {"mostra emergenza", VoiceIntent::show_emergency},
        {"mostra fraseologia", VoiceIntent::show_phraseology},
        {"nascondi fraseologia", VoiceIntent::hide_phraseology},
        {"italiano", VoiceIntent::select_italian},
        {"inglese", VoiceIntent::select_english},
        {"silenzia", VoiceIntent::mute_noncritical},
        {"riattiva audio", VoiceIntent::unmute_noncritical},
    };
    return grammar;
}

const Grammar& english_grammar() {
    static const Grammar grammar{
        {"confirm", VoiceIntent::acknowledge},
        {"done", VoiceIntent::complete},
        {"repeat", VoiceIntent::repeat},
        {"next item", VoiceIntent::next_item},
        {"skip", VoiceIntent::skip},
        {"unable", VoiceIntent::unable},
        {"cancel", VoiceIntent::cancel},
        {"show emergency", VoiceIntent::show_emergency},
        {"show phraseology", VoiceIntent::show_phraseology},
        {"hide phraseology", VoiceIntent::hide_phraseology},
        {"italian", VoiceIntent::select_italian},
        {"english", VoiceIntent::select_english},
        {"mute", VoiceIntent::mute_noncritical},
        {"unmute audio", VoiceIntent::unmute_noncritical},
    };
    return grammar;
}

bool valid_field_name(std::string_view name) {
    if (name.empty()) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char value) {
        return std::isupper(value) != 0 || std::isdigit(value) != 0 || value == '_';
    });
}

bool has_metadata(const PhraseologyTemplate& phrase) {
    return !phrase.id.empty() && !phrase.jurisdiction.empty() &&
           !phrase.revision.empty() && !phrase.source_uri.empty() &&
           !phrase.checksum.empty() && !phrase.text.empty();
}

}  // namespace

VoiceCommandGate::VoiceCommandGate(VoiceCommandConfig config) : config_(config) {
    if (!std::isfinite(config_.minimum_confidence) ||
        config_.minimum_confidence < 0.0 || config_.minimum_confidence > 1.0) {
        throw std::invalid_argument{"voice confidence threshold must be in [0,1]"};
    }
}

VoiceDecision VoiceCommandGate::evaluate(const RecognitionCandidate& candidate) const {
    VoiceDecision decision;
    decision.simulated = candidate.simulated;
    decision.normalized_transcript = normalize_transcript(candidate.transcript);

    if (!config_.enabled) {
        decision.state = RecognitionState::disabled;
        decision.reason = "VOICE_DISABLED";
        return decision;
    }
    if (!candidate.recognizer_healthy) {
        decision.state = RecognitionState::unavailable;
        decision.reason = "RECOGNIZER_UNHEALTHY";
        return decision;
    }
    if (config_.push_to_talk_required && !candidate.push_to_talk_active) {
        decision.state = RecognitionState::standby;
        decision.reason = "PUSH_TO_TALK_REQUIRED";
        return decision;
    }
    if (!std::isfinite(candidate.confidence) || candidate.confidence < 0.0 ||
        candidate.confidence > 1.0) {
        decision.state = RecognitionState::rejected;
        decision.reason = "INVALID_CONFIDENCE";
        return decision;
    }
    if (candidate.confidence < config_.minimum_confidence) {
        decision.state = RecognitionState::uncertain;
        decision.reason = "CONFIDENCE_BELOW_THRESHOLD";
        return decision;
    }
    const auto& grammar = candidate.language == Language::italian
        ? italian_grammar() : english_grammar();
    const auto match = grammar.find(decision.normalized_transcript);
    if (match == grammar.end()) {
        decision.state = RecognitionState::rejected;
        decision.reason = "OUTSIDE_APPROVED_GRAMMAR";
        return decision;
    }

    decision.state = RecognitionState::accepted;
    decision.intent = match->second;
    decision.accepted = true;
    decision.reason = "PILOT_INTERFACE_INTENT_ACCEPTED";
    return decision;
}

AcknowledgementTracker::AcknowledgementTracker(std::string item_id) {
    if (item_id.empty()) throw std::invalid_argument{"acknowledgement item id is required"};
    record_.item_id = std::move(item_id);
}

bool AcknowledgementTracker::apply_voice_decision(const VoiceDecision& decision) noexcept {
    if (!record_.presented || !decision.accepted) return false;
    if (decision.intent != VoiceIntent::acknowledge &&
        decision.intent != VoiceIntent::complete) return false;
    record_.pilot_acknowledged = true;
    record_.acknowledged_by_voice = true;
    return true;
}

void AcknowledgementTracker::acknowledge_by_touch() noexcept {
    if (record_.presented) record_.pilot_acknowledged = true;
}

void AcknowledgementTracker::set_sensor_verification(SensorVerification verification) noexcept {
    record_.sensor_verification = verification;
}

PhraseologyOutput PhraseologyEngine::compose(
    const PhraseologyTemplate& phrase,
    const PhraseologyContext& context) const {
    PhraseologyOutput output;
    output.simulated = phrase.simulated;

    if (!has_metadata(phrase)) {
        output.reason = "PACKAGE_METADATA_INCOMPLETE";
        return output;
    }
    if (!phrase.approved && !phrase.simulated) {
        output.reason = "PACKAGE_NOT_APPROVED";
        return output;
    }

    for (const auto& field : phrase.required_fields) {
        if (!valid_field_name(field)) {
            output.reason = "INVALID_FIELD_NAME";
            return output;
        }
        const auto value = context.find(field);
        if (value == context.end() || value->second.empty()) output.missing_fields.push_back(field);
    }
    if (!output.missing_fields.empty()) {
        output.reason = "REQUIRED_CONTEXT_MISSING";
        return output;
    }

    output.rendered_text = phrase.text;
    for (const auto& field : phrase.required_fields) {
        const std::string token = "{" + field + "}";
        std::size_t position = 0;
        while ((position = output.rendered_text.find(token, position)) != std::string::npos) {
            const auto& value = context.at(field);
            output.rendered_text.replace(position, token.size(), value);
            position += value.size();
        }
    }
    if (output.rendered_text.find('{') != std::string::npos ||
        output.rendered_text.find('}') != std::string::npos) {
        output.rendered_text.clear();
        output.reason = "UNRESOLVED_TEMPLATE_TOKEN";
        return output;
    }

    for (const auto& field : phrase.readback_fields) {
        if (!valid_field_name(field)) {
            output.rendered_text.clear();
            output.reason = "INVALID_READBACK_FIELD";
            return output;
        }
        const auto value = context.find(field);
        if (value == context.end() || value->second.empty()) {
            output.rendered_text.clear();
            output.missing_fields.push_back(field);
            output.reason = "READBACK_CONTEXT_MISSING";
            return output;
        }
        output.readback_values.push_back(field + "=" + value->second);
    }

    output.state = phrase.simulated
        ? PhraseologyState::simulation_draft
        : PhraseologyState::ready_for_pilot_review;
    output.reason = phrase.simulated ? "SIMULATION_DRAFT" : "PILOT_REVIEW_REQUIRED";
    return output;
}

std::string normalize_transcript(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    bool pending_space = false;
    for (const unsigned char value : text) {
        if (std::isalnum(value) != 0) {
            if (pending_space && !normalized.empty()) normalized.push_back(' ');
            normalized.push_back(static_cast<char>(std::tolower(value)));
            pending_space = false;
        } else if (!normalized.empty()) {
            pending_space = true;
        }
    }
    return normalized;
}

const char* voice_intent_name(VoiceIntent intent) noexcept {
    switch (intent) {
        case VoiceIntent::none: return "NONE";
        case VoiceIntent::acknowledge: return "ACKNOWLEDGE";
        case VoiceIntent::complete: return "COMPLETE";
        case VoiceIntent::repeat: return "REPEAT";
        case VoiceIntent::next_item: return "NEXT_ITEM";
        case VoiceIntent::skip: return "SKIP";
        case VoiceIntent::unable: return "UNABLE";
        case VoiceIntent::cancel: return "CANCEL";
        case VoiceIntent::show_emergency: return "SHOW_EMERGENCY";
        case VoiceIntent::show_phraseology: return "SHOW_PHRASEOLOGY";
        case VoiceIntent::hide_phraseology: return "HIDE_PHRASEOLOGY";
        case VoiceIntent::select_italian: return "SELECT_ITALIAN";
        case VoiceIntent::select_english: return "SELECT_ENGLISH";
        case VoiceIntent::mute_noncritical: return "MUTE_NONCRITICAL";
        case VoiceIntent::unmute_noncritical: return "UNMUTE_NONCRITICAL";
    }
    return "UNKNOWN";
}

const char* recognition_state_name(RecognitionState state) noexcept {
    switch (state) {
        case RecognitionState::disabled: return "DISABLED";
        case RecognitionState::unavailable: return "UNAVAILABLE";
        case RecognitionState::standby: return "STANDBY";
        case RecognitionState::listening: return "LISTENING";
        case RecognitionState::accepted: return "ACCEPTED";
        case RecognitionState::uncertain: return "UNCERTAIN";
        case RecognitionState::rejected: return "REJECTED";
    }
    return "UNKNOWN";
}

const char* phraseology_state_name(PhraseologyState state) noexcept {
    switch (state) {
        case PhraseologyState::blocked: return "BLOCKED";
        case PhraseologyState::simulation_draft: return "SIMULATION_DRAFT";
        case PhraseologyState::ready_for_pilot_review: return "READY_FOR_PILOT_REVIEW";
    }
    return "UNKNOWN";
}

}  // namespace efis::voice
