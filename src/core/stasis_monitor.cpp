#include "efis/core/stasis_monitor.hpp"

#include <algorithm>
#include <cmath>

namespace efis {
namespace {

void publish(std::vector<StasisState>& states, const StasisState& state) {
    const auto it = std::find_if(states.begin(), states.end(),
                                 [&](const StasisState& s) { return s.signal == state.signal; });
    if (it == states.end()) {
        states.push_back(state);
        return;
    }
    *it = state;
}

}  // namespace

void StasisMonitor::configure(SignalId signal, StasisPolicy policy) {
    if (signal == SignalId::unknown) return;
    if (Entry* existing = find(signal); existing != nullptr) {
        existing->policy = policy;
        return;
    }
    Entry entry{};
    entry.signal = signal;
    entry.policy = policy;
    entry.state.signal = signal;
    entries_.push_back(entry);
}

StasisMonitor::Entry* StasisMonitor::find(SignalId signal) noexcept {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [signal](const Entry& e) { return e.signal == signal; });
    return it == entries_.end() ? nullptr : &*it;
}

Motion StasisMonitor::observe(const Sample<double>& sample, MonoTime now) {
    Entry* entry = find(sample.origin.signal);
    if (entry == nullptr || !entry->policy.applicable) return Motion::unknown;

    // Un dato gia' scaduto o assente non viene sorvegliato: la sua immobilita'
    // e' una conseguenza del guasto che e' stato gia' rilevato, non un secondo
    // guasto. Contarla produrrebbe due segnalazioni per una sola causa.
    if (!sample.usable()) {
        entry->seen = false;
        entry->state.motion = Motion::unknown;
        entry->state.still_for = std::chrono::milliseconds::zero();
        publish(states_, entry->state);
        return Motion::unknown;
    }

    if (!entry->seen) {
        entry->seen = true;
        entry->state.reference = sample.value;
        entry->state.moved_at = now;
        entry->state.motion = Motion::unknown;
        entry->state.still_for = std::chrono::milliseconds::zero();
        publish(states_, entry->state);
        return Motion::unknown;
    }

    // Il confronto e' sulla risoluzione dichiarata, non sull'uguaglianza esatta:
    // un sensore quantizzato ripete lo stesso numero senza per questo essere
    // fermo, e un rumore di un bit non e' movimento.
    const double change = std::abs(sample.value - entry->state.reference);
    if (change > entry->policy.resolution) {
        entry->state.reference = sample.value;
        entry->state.moved_at = now;
        entry->state.motion = Motion::moving;
        entry->state.still_for = std::chrono::milliseconds::zero();
        publish(states_, entry->state);
        return Motion::moving;
    }

    const auto still = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - entry->state.moved_at);
    entry->state.still_for = still;
    entry->state.motion = still >= entry->policy.suspect_after
        ? Motion::suspect
        : Motion::moving;
    publish(states_, entry->state);
    return entry->state.motion;
}

Motion StasisMonitor::motion(SignalId signal) const noexcept {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [signal](const Entry& e) { return e.signal == signal; });
    return it == entries_.end() ? Motion::unknown : it->state.motion;
}

void StasisMonitor::reset() noexcept {
    for (Entry& entry : entries_) {
        entry.seen = false;
        entry.state.motion = Motion::unknown;
        entry.state.still_for = std::chrono::milliseconds::zero();
    }
    states_.clear();
}

}  // namespace efis
