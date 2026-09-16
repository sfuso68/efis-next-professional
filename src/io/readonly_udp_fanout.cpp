#include "efis/io/readonly_udp_fanout.hpp"

#include <array>
#include <charconv>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace efis::io {
namespace {

bool is_explicit_unicast(std::string_view address) {
    std::array<unsigned int, 4> octets{};
    std::size_t start{};
    for (std::size_t index = 0; index < octets.size(); ++index) {
        const auto end = address.find('.', start);
        if ((end == std::string_view::npos) != (index == octets.size() - 1)) {
            return false;
        }
        const auto token_end = end == std::string_view::npos ? address.size() : end;
        if (token_end == start) return false;
        const auto token = address.substr(start, token_end - start);
        unsigned int value{};
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            value > 255) {
            return false;
        }
        octets[index] = value;
        start = token_end + 1;
    }
    // Exclude unspecified/this-network, multicast and both global and the
    // /24-directed broadcast form used by the service LAN policy.
    return octets[0] != 0 && octets[0] < 224 && octets[3] != 255;
}

}  // namespace

ReadOnlyUdpFanout::ReadOnlyUdpFanout(std::vector<UdpEndpoint> destinations) {
    if (destinations.empty()) {
        throw std::invalid_argument{"at least one read-only UDP target is required"};
    }
    if (destinations.size() > 8) {
        throw std::invalid_argument{"read-only UDP fan-out is limited to eight targets"};
    }
    std::set<std::pair<std::string, std::uint16_t>> unique;
    targets_.reserve(destinations.size());
    for (auto& destination : destinations) {
        const auto key = std::pair{destination.address, destination.port};
        if (!unique.insert(key).second) {
            throw std::invalid_argument{"duplicate read-only UDP target"};
        }
        if (!is_explicit_unicast(destination.address)) {
            throw std::invalid_argument{"read-only UDP target must be explicit unicast"};
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
