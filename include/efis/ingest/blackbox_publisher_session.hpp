#pragma once

#include "efis/ingest/mavlink_pipeline.hpp"
#include "efis/providers/ddp_udp_publisher.hpp"
#include "efis/recording/segmented_recorder.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace efis::ingest {

struct BlackboxPublisherStatistics final {
    std::uint64_t mavlink_bytes{};
    std::uint64_t can_frames{};
    std::uint64_t snapshots_published{};
    std::uint64_t snapshots_recorded{};
    std::uint64_t node_events_observed{};
    std::uint64_t source_events_observed{};
    std::uint64_t ddp_publish_failures{};
};

struct BlackboxDisplayStatistics final {
    io::UdpEndpoint destination;
    DdpUdpPublisherMetrics transmitted{};
    std::uint64_t failures{};
    std::string last_error;
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
    explicit BlackboxPublisherSession(
        std::vector<DdpUdpPublisherConfig> publishers,
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
    [[nodiscard]] DdpUdpPublisherMetrics publisher_metrics() const noexcept;
    [[nodiscard]] std::vector<BlackboxDisplayStatistics>
        display_statistics() const;

private:
    struct DisplayPublisher final {
        io::UdpEndpoint destination;
        std::unique_ptr<DdpUdpPublisher> publisher;
        std::uint64_t failures{};
        std::string last_error;
    };

    MavlinkPipeline pipeline_;
    std::vector<DisplayPublisher> publishers_;
    recording::SegmentedRecorder* recorder_{};
    BlackboxPublisherStatistics statistics_{};
};

}  // namespace efis::ingest
