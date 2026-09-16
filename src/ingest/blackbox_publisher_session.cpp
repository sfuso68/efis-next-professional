#include "efis/ingest/blackbox_publisher_session.hpp"

#include <set>
#include <stdexcept>
#include <utility>

namespace efis::ingest {

BlackboxPublisherSession::BlackboxPublisherSession(
    DdpUdpPublisherConfig publisher, PipelinePolicy policy,
    recording::SegmentedRecorder* recorder)
    : BlackboxPublisherSession(
          std::vector<DdpUdpPublisherConfig>{std::move(publisher)}, policy, recorder) {}

BlackboxPublisherSession::BlackboxPublisherSession(
    std::vector<DdpUdpPublisherConfig> publishers, PipelinePolicy policy,
    recording::SegmentedRecorder* recorder)
    : pipeline_(policy), recorder_(recorder) {
    if (publishers.empty()) {
        throw std::invalid_argument{"at least one DDP display is required"};
    }
    const auto identity = publishers.front().session;
    std::set<std::pair<std::string, std::uint16_t>> destinations;
    publishers_.reserve(publishers.size());
    for (auto& config : publishers) {
        if (config.session.profile != identity.profile ||
            config.session.source_node != identity.source_node ||
            config.session.publisher_instance != identity.publisher_instance ||
            config.session.first_sequence != identity.first_sequence) {
            throw std::invalid_argument{"all DDP displays must share publisher identity"};
        }
        const auto key = std::pair{config.destination.address, config.destination.port};
        if (!destinations.insert(key).second) {
            throw std::invalid_argument{"duplicate DDP display destination"};
        }
        auto destination = config.destination;
        publishers_.push_back({std::move(destination),
                               std::make_unique<DdpUdpPublisher>(std::move(config)),
                               0, {}});
    }
}

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
    for (auto& display : publishers_) {
        try {
            display.publisher->publish(pipeline_.state(), now);
            display.last_error.clear();
        } catch (const std::exception& error) {
            ++display.failures;
            ++statistics_.ddp_publish_failures;
            display.last_error = error.what();
        }
    }
    ++statistics_.snapshots_published;
    // Gli eventi sono stati registrati e/o contati: lasciarli in coda li
    // duplicherebbe al ciclo seguente e farebbe crescere il processo.
    pipeline_.clear_events();
}

DdpUdpPublisherMetrics BlackboxPublisherSession::publisher_metrics() const noexcept {
    DdpUdpPublisherMetrics aggregate{};
    for (const auto& display : publishers_) {
        aggregate.datagrams += display.publisher->metrics().datagrams;
        aggregate.bytes += display.publisher->metrics().bytes;
    }
    return aggregate;
}

std::vector<BlackboxDisplayStatistics>
BlackboxPublisherSession::display_statistics() const {
    std::vector<BlackboxDisplayStatistics> statistics;
    statistics.reserve(publishers_.size());
    for (const auto& display : publishers_) {
        statistics.push_back({display.destination, display.publisher->metrics(),
                              display.failures, display.last_error});
    }
    return statistics;
}

}  // namespace efis::ingest
