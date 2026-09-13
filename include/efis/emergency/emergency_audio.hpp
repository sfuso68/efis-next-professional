#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace efis::emergency {

enum class AudioCuePriority : std::uint8_t {
    advisory = 0,
    caution = 1,
    warning = 2,
    critical = 3
};

struct AudioCue final {
    std::string id;
    std::string text;
    AudioCuePriority priority{AudioCuePriority::advisory};
    std::chrono::milliseconds repeat_inhibit{3000};
};

struct AudioArbiterOutput final {
    std::optional<AudioCue> active;
    std::size_t queued{};
    bool interrupted{};
    bool noncritical_muted{};
};

// Chooses one cue at a time. Playback remains an external adapter and must
// report completion; the arbiter itself has no audio-device dependency.
class EmergencyAudioArbiter final {
public:
    void set_noncritical_muted(bool muted) noexcept;
    [[nodiscard]] bool enqueue(AudioCue cue, MonoTime now);
    void complete_active(MonoTime now);
    void clear_noncritical() noexcept;

    [[nodiscard]] const AudioArbiterOutput& output() const noexcept { return output_; }

private:
    struct Pending final {
        AudioCue cue;
        std::uint64_t order{};
    };

    void select_next();
    [[nodiscard]] bool repeat_blocked(const AudioCue& cue, MonoTime now) const;

    AudioArbiterOutput output_{};
    std::vector<Pending> pending_;
    std::unordered_map<std::string, MonoTime> last_completed_;
    std::uint64_t next_order_{1};
};

[[nodiscard]] const char* audio_priority_name(AudioCuePriority priority) noexcept;

}  // namespace efis::emergency
