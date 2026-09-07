#pragma once

#include "efis/ingest/mavlink_pipeline.hpp"
#include "efis/providers/ddp_udp_publisher.hpp"
#include "efis/recording/segmented_recorder.hpp"

#include <cstdint>
#include <span>

namespace efis::ingest {

struct BlackboxPublisherStatistics final {
    std::uint64_t mavlink_bytes{};
    std::uint64_t can_frames{};
    std::uint64_t snapshots_published{};
    std::uint64_t snapshots_recorded{};
    std::uint64_t node_events_observed{};
    std::uint64_t source_events_observed{};
};

// Punto unico di composizione del data plane Blackbox. Riceve soltanto dati
// provenienti dai trasporti monitor-only, li fa passare nella pipeline protetta
// e distribuisce ai display esclusivamente lo snapshot normalizzato DDP V2.
// Non espone alcun metodo di scrittura verso MAVLink o CAN.
class BlackboxPublisherSession final {
public:
    explicit BlackboxPublisherSession(DdpUdpPublisherConfig publisher,
                                      PipelinePolicy policy = {},
                                      recording::SegmentedRecorder* recorder = nullptr);

    void consume_mavlink(std::span<const std::uint8_t> bytes, MonoTime now);
    void consume_can(std::span<const can::Frame> frames, MonoTime now);
    void publish(MonoTime now);

    [[nodiscard]] const MavlinkPipeline& pipeline() const noexcept {
        return pipeline_;
    }
    [[nodiscard]] const BlackboxPublisherStatistics& statistics() const noexcept {
        return statistics_;
    }
    [[nodiscard]] const DdpUdpPublisherMetrics& publisher_metrics() const noexcept {
        return publisher_.metrics();
    }

private:
    MavlinkPipeline pipeline_;
    DdpUdpPublisher publisher_;
    recording::SegmentedRecorder* recorder_{};
    BlackboxPublisherStatistics statistics_{};
};

}  // namespace efis::ingest
