#include "efis/providers/ddp_udp_source.hpp"

#include <algorithm>
#include <utility>

namespace efis {
namespace {

template <class T>
void mark(Sample<T>& sample, Validity validity) noexcept {
    if (sample.usable() || sample.validity == Validity::stale) {
        sample.validity = validity;
    }
}

}  // namespace

DdpUdpFlightSource::DdpUdpFlightSource(DdpUdpSourceConfig config)
    : config_(std::move(config)),
      receiver_(config_.local),
      consumer_(config_.consumer) {
    if (config_.max_datagrams_per_poll == 0) config_.max_datagrams_per_poll = 1;
}

FlightSnapshot DdpUdpFlightSource::poll(MonoTime now,
                                        std::chrono::milliseconds) {
    for (std::size_t count = 0; count < config_.max_datagrams_per_poll; ++count) {
        const auto result = receiver_.receive(buffer_);
        if (result.status == io::DatagramStatus::would_block) break;
        if (result.status == io::DatagramStatus::truncated) {
            ++truncated_datagrams_;
            continue;
        }
        if (result.status == io::DatagramStatus::failed) {
            ++socket_errors_;
            break;
        }
        (void)consumer_.consume(
            std::span<const std::uint8_t>{buffer_.data(), result.size}, now);
    }

    const auto* latest = consumer_.latest();
    if (latest == nullptr) return {};
    auto output = *latest;
    switch (consumer_.state(now)) {
        case protocol::ddp::LinkState::waiting: return {};
        case protocol::ddp::LinkState::live: return output;
        case protocol::ddp::LinkState::stale:
            mark_unusable(output, Validity::stale);
            output.health.level = std::max(output.health.level, HealthLevel::caution);
            return output;
        case protocol::ddp::LinkState::lost:
            mark_unusable(output, Validity::unavailable);
            output.health.level = HealthLevel::warning;
            return output;
    }
    return {};
}

void DdpUdpFlightSource::mark_unusable(FlightSnapshot& snapshot,
                                       Validity validity) noexcept {
    mark(snapshot.attitude.roll_deg, validity);
    mark(snapshot.attitude.pitch_deg, validity);
    mark(snapshot.attitude.heading_deg, validity);
    mark(snapshot.attitude.turn_rate_deg_s, validity);
    mark(snapshot.attitude.lateral_acceleration_g, validity);
    mark(snapshot.air_data.indicated_airspeed_kt, validity);
    mark(snapshot.air_data.pressure_altitude_ft, validity);
    mark(snapshot.air_data.vertical_speed_fpm, validity);
    mark(snapshot.air_data.baro_setting_hpa, validity);
    mark(snapshot.air_data.indicated_altitude_ft, validity);
    mark(snapshot.position.latitude_deg, validity);
    mark(snapshot.position.longitude_deg, validity);
    mark(snapshot.position.ground_speed_kt, validity);
    mark(snapshot.position.gnss_vertical_speed_fpm, validity);
    mark(snapshot.guidance.flight_director_active, validity);
    mark(snapshot.guidance.commanded_roll_deg, validity);
    mark(snapshot.guidance.commanded_pitch_deg, validity);
    mark(snapshot.guidance.selected_altitude_ft, validity);
    mark(snapshot.guidance.selected_course_deg, validity);
    mark(snapshot.guidance.selected_airspeed_kt, validity);
    mark(snapshot.navigation.cdi_deviation_dots, validity);
    mark(snapshot.navigation.glide_slope_dots, validity);
    mark(snapshot.navigation.to_flag, validity);
}

}  // namespace efis
