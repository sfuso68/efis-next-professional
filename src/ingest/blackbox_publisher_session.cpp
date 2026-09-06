#include "efis/ingest/blackbox_publisher_session.hpp"

#include <utility>

namespace efis::ingest {

BlackboxPublisherSession::BlackboxPublisherSession(
    DdpUdpPublisherConfig publisher, PipelinePolicy policy,
    recording::SegmentedRecorder* recorder)
    : pipeline_(policy), publisher_(std::move(publisher)), recorder_(recorder) {}

void BlackboxPublisherSession::consume_mavlink(
    std::span<const std::uint8_t> bytes, MonoTime now) {
    pipeline_.consume(bytes, now);
    statistics_.mavlink_bytes += bytes.size();
}

void BlackboxPublisherSession::consume_can(
    std::span<const can::Frame> frames, MonoTime now) {
    pipeline_.consume_can(frames, now);
    statistics_.can_frames += frames.size();
}

void BlackboxPublisherSession::publish(MonoTime now) {
    // Anche senza nuovi frame il tempo deve avanzare: e' qui che un sensore
    // silenzioso diventa stale/lost prima che lo snapshot raggiunga i display.
    pipeline_.update(now);
    const auto node_events = pipeline_.node_events();
    const auto& source_events = pipeline_.source_events();
    statistics_.node_events_observed += node_events.size();
    statistics_.source_events_observed += source_events.size();
    if (recorder_ != nullptr) {
        for (const auto& event : node_events) recorder_->append(event);
        for (const auto& event : source_events) recorder_->append(event);
        recorder_->append(pipeline_.state());
        ++statistics_.snapshots_recorded;
    }
    publisher_.publish(pipeline_.state(), now);
    ++statistics_.snapshots_published;
    // Gli eventi sono stati registrati e/o contati: lasciarli in coda li
    // duplicherebbe al ciclo seguente e farebbe crescere il processo.
    pipeline_.clear_events();
}

}  // namespace efis::ingest
