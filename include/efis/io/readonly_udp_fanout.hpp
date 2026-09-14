#pragma once

#include "efis/io/udp_datagram.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace efis::io {

struct ReadOnlyUdpTargetStatistics final {
    UdpEndpoint destination;
    std::uint64_t datagrams{};
    std::uint64_t bytes{};
    std::uint64_t failures{};
    std::string last_error;
};

struct ReadOnlyUdpFanoutStatistics final {
    std::uint64_t datagrams{};
    std::uint64_t bytes{};
    std::uint64_t failures{};
};

// Fan-out deliberatamente privo di receiver: inoltra copie di datagrammi a
// endpoint unicast espliciti e non puo' creare un percorso di ritorno verso la
// sorgente avionica. Non e' un router IP ne' un bridge di rete.
class ReadOnlyUdpFanout final {
public:
    explicit ReadOnlyUdpFanout(std::vector<UdpEndpoint> destinations);

    void forward(std::span<const std::uint8_t> datagram) noexcept;
    [[nodiscard]] ReadOnlyUdpFanoutStatistics statistics() const noexcept;
    [[nodiscard]] std::vector<ReadOnlyUdpTargetStatistics>
        target_statistics() const;

private:
    struct Target final {
        UdpEndpoint destination;
        std::unique_ptr<UdpSender> sender;
        std::uint64_t datagrams{};
        std::uint64_t bytes{};
        std::uint64_t failures{};
        std::string last_error;
    };

    std::vector<Target> targets_;
};

}  // namespace efis::io
