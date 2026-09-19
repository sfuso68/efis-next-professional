#include "efis/io/readonly_udp_fanout.hpp"

#include <array>
#include <charconv>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace efis::io {
namespace {

std::array<unsigned int, 4> parse_ipv4(std::string_view address) {
    std::array<unsigned int, 4> octets{};
    std::size_t start{};
    for (std::size_t index = 0; index < octets.size(); ++index) {
        const auto end = address.find('.', start);
        if ((end == std::string_view::npos) != (index == octets.size() - 1)) {
            throw std::invalid_argument{"read-only UDP target must be numeric IPv4"};
        }
        const auto token_end = end == std::string_view::npos ? address.size() : end;
        if (token_end == start) {
            throw std::invalid_argument{"read-only UDP target must be numeric IPv4"};
        }
        const auto token = address.substr(start, token_end - start);
        unsigned int value{};
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            value > 255) {
            throw std::invalid_argument{"read-only UDP target must be numeric IPv4"};
        }
        octets[index] = value;
        start = token_end + 1;
    }
    return octets;
}

bool is_explicit_unicast(const std::array<unsigned int, 4>& octets) {
    // Exclude unspecified/this-network, multicast and both global and the
    // /24-directed broadcast form used by the service LAN policy.
    return octets[0] != 0 && octets[0] < 224 && octets[3] != 255;
}

bool is_flight_data_subnet(const std::array<unsigned int, 4>& octets) {
    // ADR-015 freezes 10.77.10.0/24 as FLIGHT-DATA. Observer traffic must
    // never be emitted into that subnet, even if an environment-file typo
    // names the Pixhawk or another avionics node as an observer.
    return octets[0] == 10 && octets[1] == 77 && octets[2] == 10;
}

}  // namespace

ReadOnlyUdpFanout::ReadOnlyUdpFanout(std::vector<UdpEndpoint> destinations,
                                     std::string protected_source) {
    if (destinations.empty()) {
        throw std::invalid_argument{"at least one read-only UDP target is required"};
    }
    if (destinations.size() > 8) {
        throw std::invalid_argument{"read-only UDP fan-out is limited to eight targets"};
    }
    std::array<unsigned int, 4> protected_source_octets{};
    const bool has_protected_source = !protected_source.empty();
    if (has_protected_source) {
        protected_source_octets = parse_ipv4(protected_source);
        if (!is_explicit_unicast(protected_source_octets)) {
            throw std::invalid_argument{
                "protected MAVLink source must be explicit unicast"};
        }
    }
    std::set<std::pair<std::array<unsigned int, 4>, std::uint16_t>> unique;
    targets_.reserve(destinations.size());
    for (auto& destination : destinations) {
        const auto octets = parse_ipv4(destination.address);
        if (!is_explicit_unicast(octets)) {
            throw std::invalid_argument{"read-only UDP target must be explicit unicast"};
        }
        if (is_flight_data_subnet(octets)) {
            throw std::invalid_argument{
                "read-only UDP target must not be in FLIGHT-DATA 10.77.10.0/24"};
        }
        if (has_protected_source && octets == protected_source_octets) {
            throw std::invalid_argument{
                "read-only UDP target must not equal the protected MAVLink source"};
        }
        const auto key = std::pair{octets, destination.port};
        if (!unique.insert(key).second) {
            throw std::invalid_argument{"duplicate read-only UDP target"};
        }
        auto retained = destination;
        targets_.push_back({std::move(retained),
                            std::make_unique<UdpSender>(std::move(destination)),
                            0, 0, 0, {}});
    }
}

void ReadOnlyUdpFanout::forward(
    std::span<const std::uint8_t> datagram) noexcept {
    for (auto& target : targets_) {
        try {
            target.sender->send(datagram);
            ++target.datagrams;
            target.bytes += datagram.size();
            target.last_error.clear();
        } catch (const std::exception& error) {
            ++target.failures;
            target.last_error = error.what();
        }
    }
}

ReadOnlyUdpFanoutStatistics ReadOnlyUdpFanout::statistics() const noexcept {
    ReadOnlyUdpFanoutStatistics result{};
    for (const auto& target : targets_) {
        result.datagrams += target.datagrams;
        result.bytes += target.bytes;
        result.failures += target.failures;
    }
    return result;
}

std::vector<ReadOnlyUdpTargetStatistics>
ReadOnlyUdpFanout::target_statistics() const {
    std::vector<ReadOnlyUdpTargetStatistics> result;
    result.reserve(targets_.size());
    for (const auto& target : targets_) {
        result.push_back({target.destination, target.datagrams, target.bytes,
                          target.failures, target.last_error});
    }
    return result;
}

}  // namespace efis::io
