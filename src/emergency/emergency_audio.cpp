#include "efis/emergency/emergency_audio.hpp"

#include <algorithm>

namespace efis::emergency {

void EmergencyAudioArbiter::set_noncritical_muted(bool muted) noexcept {
    output_.noncritical_muted = muted;
    if (muted) clear_noncritical();
}

bool EmergencyAudioArbiter::repeat_blocked(const AudioCue& cue, MonoTime now) const {
    const auto previous = last_completed_.find(cue.id);
    return previous != last_completed_.end() && now - previous->second < cue.repeat_inhibit;
}

bool EmergencyAudioArbiter::enqueue(AudioCue cue, MonoTime now) {
    if (cue.id.empty() || cue.text.empty() || cue.repeat_inhibit.count() < 0) return false;
    if (output_.noncritical_muted && cue.priority < AudioCuePriority::warning) return false;
    if (repeat_blocked(cue, now)) return false;
    if (output_.active && output_.active->id == cue.id) return false;
    if (std::any_of(pending_.begin(), pending_.end(), [&](const Pending& value) {
            return value.cue.id == cue.id;
        })) return false;

    output_.interrupted = false;
    if (output_.active && cue.priority > output_.active->priority) {
        pending_.push_back(Pending{*output_.active, next_order_++});
        output_.active = std::move(cue);
        output_.interrupted = true;
    } else if (!output_.active) {
        output_.active = std::move(cue);
    } else {
        pending_.push_back(Pending{std::move(cue), next_order_++});
    }
    output_.queued = pending_.size();
    return true;
}

void EmergencyAudioArbiter::complete_active(MonoTime now) {
    if (output_.active) last_completed_[output_.active->id] = now;
    output_.active.reset();
    output_.interrupted = false;
    select_next();
}

void EmergencyAudioArbiter::clear_noncritical() noexcept {
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(), [](const Pending& value) {
        return value.cue.priority < AudioCuePriority::warning;
    }), pending_.end());
    if (output_.active && output_.active->priority < AudioCuePriority::warning) {
        output_.active.reset();
        select_next();
    }
    output_.queued = pending_.size();
}

void EmergencyAudioArbiter::select_next() {
    if (pending_.empty()) {
        output_.queued = 0;
        return;
    }
    const auto selected = std::max_element(pending_.begin(), pending_.end(),
        [](const Pending& left, const Pending& right) {
            if (left.cue.priority != right.cue.priority)
                return left.cue.priority < right.cue.priority;
            return left.order > right.order;
        });
    output_.active = selected->cue;
    pending_.erase(selected);
    output_.queued = pending_.size();
}

const char* audio_priority_name(AudioCuePriority priority) noexcept {
    switch (priority) {
        case AudioCuePriority::advisory: return "ADVISORY";
        case AudioCuePriority::caution: return "CAUTION";
        case AudioCuePriority::warning: return "WARNING";
        case AudioCuePriority::critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

}  // namespace efis::emergency
