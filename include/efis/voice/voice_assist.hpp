#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace efis::voice {

enum class Language : std::uint8_t {
    italian,
    english
};

enum class VoiceIntent : std::uint8_t {
    none,
    acknowledge,
    complete,
    repeat,
    next_item,
    skip,
    unable,
    cancel,
    show_emergency,
    show_phraseology,
    hide_phraseology,
    select_italian,
    select_english,
    mute_noncritical,
    unmute_noncritical
};

enum class RecognitionState : std::uint8_t {
    disabled,
    unavailable,
    standby,
    listening,
    accepted,
    uncertain,
    rejected
};

struct VoiceCommandConfig final {
    bool enabled{};
    bool push_to_talk_required{true};
    double minimum_confidence{0.82};
};

struct RecognitionCandidate final {
    Language language{Language::italian};
    std::string transcript;
    double confidence{};
    bool push_to_talk_active{};
    bool recognizer_healthy{};
    bool simulated{};
};

struct VoiceDecision final {
    RecognitionState state{RecognitionState::standby};
    VoiceIntent intent{VoiceIntent::none};
    std::string normalized_transcript;
    std::string reason;
    bool accepted{};
    bool simulated{};
    // A decision is a pilot-interface event only. This remains false even for
    // accepted grammar entries: no radio, Pixhawk or actuator command exists.
    bool transmission_or_actuation_allowed{};
};

class VoiceCommandGate final {
public:
    explicit VoiceCommandGate(VoiceCommandConfig config = {});

    void set_enabled(bool enabled) noexcept { config_.enabled = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return config_.enabled; }
    [[nodiscard]] VoiceDecision evaluate(const RecognitionCandidate& candidate) const;

private:
    VoiceCommandConfig config_;
};

enum class SensorVerification : std::uint8_t {
    pending,
    verified,
    failed,
    not_verifiable
};

struct AcknowledgementRecord final {
    std::string item_id;
    bool presented{};
    bool pilot_acknowledged{};
    bool acknowledged_by_voice{};
    SensorVerification sensor_verification{SensorVerification::pending};
};

class AcknowledgementTracker final {
public:
    explicit AcknowledgementTracker(std::string item_id);

    void mark_presented() noexcept { record_.presented = true; }
    [[nodiscard]] bool apply_voice_decision(const VoiceDecision& decision) noexcept;
    void acknowledge_by_touch() noexcept;
    void set_sensor_verification(SensorVerification verification) noexcept;
    [[nodiscard]] const AcknowledgementRecord& record() const noexcept { return record_; }

private:
    AcknowledgementRecord record_;
};

struct PhraseologyTemplate final {
    std::string id;
    Language language{Language::italian};
    std::string jurisdiction;
    std::string revision;
    std::string source_uri;
    std::string checksum;
    std::string text;
    std::vector<std::string> required_fields;
    std::vector<std::string> readback_fields;
    bool approved{};
    bool simulated{};
};

using PhraseologyContext = std::unordered_map<std::string, std::string>;

enum class PhraseologyState : std::uint8_t {
    blocked,
    simulation_draft,
    ready_for_pilot_review
};

struct PhraseologyOutput final {
    PhraseologyState state{PhraseologyState::blocked};
    std::string rendered_text;
    std::string reason;
    std::vector<std::string> missing_fields;
    std::vector<std::string> readback_values;
    bool pilot_review_required{true};
    bool transmit_allowed{};
    bool simulated{};
};

class PhraseologyEngine final {
public:
    [[nodiscard]] PhraseologyOutput compose(
        const PhraseologyTemplate& phrase,
        const PhraseologyContext& context) const;
};

[[nodiscard]] std::string normalize_transcript(std::string_view text);
[[nodiscard]] const char* voice_intent_name(VoiceIntent intent) noexcept;
[[nodiscard]] const char* recognition_state_name(RecognitionState state) noexcept;
[[nodiscard]] const char* phraseology_state_name(PhraseologyState state) noexcept;

}  // namespace efis::voice
