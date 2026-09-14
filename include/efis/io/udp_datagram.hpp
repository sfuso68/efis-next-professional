#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace efis::io {

inline constexpr std::size_t max_udp_payload = 65'507;

struct UdpEndpoint final {
    std::string address{"127.0.0.1"};
    std::uint16_t port{};
};

enum class DatagramStatus : std::uint8_t {
    received,
    would_block,
    truncated,
    failed
};

struct DatagramResult final {
    DatagramStatus status{DatagramStatus::would_block};
    std::size_t size{};
    int error_code{};
    UdpEndpoint source{};
};

class UdpSender final {
public:
    explicit UdpSender(UdpEndpoint destination);
    ~UdpSender();
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    void send(std::span<const std::uint8_t> datagram);
    [[nodiscard]] const UdpEndpoint& destination() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class UdpReceiver final {
public:
    explicit UdpReceiver(UdpEndpoint local,
                         std::size_t receive_buffer_bytes = 0);
    ~UdpReceiver();
    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    [[nodiscard]] DatagramResult receive(std::span<std::uint8_t> buffer) noexcept;
    [[nodiscard]] const UdpEndpoint& local() const noexcept;
    [[nodiscard]] std::size_t receive_buffer_bytes() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace efis::io
