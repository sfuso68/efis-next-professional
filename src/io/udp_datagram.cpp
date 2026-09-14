#include "efis/io/udp_datagram.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace efis::io {
namespace {

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
int last_error() noexcept { return WSAGetLastError(); }
void close_socket(Socket socket) noexcept { closesocket(socket); }
void ensure_runtime() {
    static std::once_flag once;
    static int result{};
    std::call_once(once, [] {
        WSADATA data{};
        result = WSAStartup(MAKEWORD(2, 2), &data);
    });
    if (result != 0) throw std::system_error{result, std::system_category(), "WSAStartup"};
}
bool make_nonblocking(Socket socket) {
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
}
bool would_block(int error) noexcept { return error == WSAEWOULDBLOCK; }
bool truncated(int error) noexcept { return error == WSAEMSGSIZE; }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
int last_error() noexcept { return errno; }
void close_socket(Socket socket) noexcept { ::close(socket); }
void ensure_runtime() {}
bool make_nonblocking(Socket socket) {
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
bool would_block(int error) noexcept { return error == EAGAIN || error == EWOULDBLOCK; }
bool truncated(int error) noexcept { return error == EMSGSIZE; }
#endif

sockaddr_in address_for(const UdpEndpoint& endpoint, bool allow_ephemeral) {
    if (endpoint.port == 0 && !allow_ephemeral) {
        throw std::invalid_argument{"UDP destination port must be non-zero"};
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.address.c_str(), &address.sin_addr) != 1) {
        throw std::invalid_argument{"UDP address must be a numeric IPv4 address"};
    }
    return address;
}

Socket make_socket() {
    ensure_runtime();
    const Socket socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == invalid_socket) {
        throw std::system_error{last_error(), std::system_category(), "UDP socket"};
    }
    return socket;
}

}  // namespace

struct UdpSender::Impl final {
    UdpEndpoint endpoint;
    Socket socket{invalid_socket};
    sockaddr_in address{};
};

UdpSender::UdpSender(UdpEndpoint destination) : impl_(std::make_unique<Impl>()) {
    impl_->endpoint = std::move(destination);
    impl_->address = address_for(impl_->endpoint, false);
    impl_->socket = make_socket();
}

UdpSender::~UdpSender() {
    if (impl_ && impl_->socket != invalid_socket) close_socket(impl_->socket);
}

void UdpSender::send(std::span<const std::uint8_t> datagram) {
    if (datagram.empty() || datagram.size() > max_udp_payload ||
        datagram.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error{"invalid UDP datagram length"};
    }
    const auto sent = ::sendto(impl_->socket,
        reinterpret_cast<const char*>(datagram.data()), static_cast<int>(datagram.size()), 0,
        reinterpret_cast<const sockaddr*>(&impl_->address), sizeof(impl_->address));
    if (sent < 0) {
        throw std::system_error{last_error(), std::system_category(), "UDP sendto"};
    }
    if (static_cast<std::size_t>(sent) != datagram.size()) {
        throw std::runtime_error{"partial UDP datagram send"};
    }
}

const UdpEndpoint& UdpSender::destination() const noexcept { return impl_->endpoint; }

struct UdpReceiver::Impl final {
    UdpEndpoint endpoint;
    Socket socket{invalid_socket};
    std::size_t receive_buffer_bytes{};
};

UdpReceiver::UdpReceiver(UdpEndpoint local, std::size_t receive_buffer_bytes)
    : impl_(std::make_unique<Impl>()) {
    impl_->endpoint = std::move(local);
    const auto address = address_for(impl_->endpoint, true);
    impl_->socket = make_socket();
    int reuse = 1;
#if defined(_WIN32)
    setsockopt(impl_->socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(impl_->socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    if (receive_buffer_bytes > 0) {
        if (receive_buffer_bytes >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            close_socket(impl_->socket);
            impl_->socket = invalid_socket;
            throw std::length_error{"UDP receive buffer is too large"};
        }
        const int requested = static_cast<int>(receive_buffer_bytes);
#if defined(_WIN32)
        const int result = setsockopt(
            impl_->socket, SOL_SOCKET, SO_RCVBUF,
            reinterpret_cast<const char*>(&requested), sizeof(requested));
#else
        const int result = setsockopt(
            impl_->socket, SOL_SOCKET, SO_RCVBUF, &requested, sizeof(requested));
#endif
        if (result != 0) {
            const int error = last_error();
            close_socket(impl_->socket);
            impl_->socket = invalid_socket;
            throw std::system_error{error, std::system_category(),
                                    "UDP SO_RCVBUF"};
        }
    }
    int actual_receive_buffer{};
#if defined(_WIN32)
    int receive_buffer_size = sizeof(actual_receive_buffer);
    const int receive_buffer_result = getsockopt(
        impl_->socket, SOL_SOCKET, SO_RCVBUF,
        reinterpret_cast<char*>(&actual_receive_buffer), &receive_buffer_size);
#else
    socklen_t receive_buffer_size = sizeof(actual_receive_buffer);
    const int receive_buffer_result = getsockopt(
        impl_->socket, SOL_SOCKET, SO_RCVBUF,
        &actual_receive_buffer, &receive_buffer_size);
#endif
    if (receive_buffer_result != 0) {
        const int error = last_error();
        close_socket(impl_->socket);
        impl_->socket = invalid_socket;
        throw std::system_error{error, std::system_category(),
                                "UDP getsockopt SO_RCVBUF"};
    }
    if (actual_receive_buffer <= 0) {
        close_socket(impl_->socket);
        impl_->socket = invalid_socket;
        throw std::runtime_error{"UDP SO_RCVBUF returned a non-positive size"};
    }
    impl_->receive_buffer_bytes =
        static_cast<std::size_t>(actual_receive_buffer);
    if (::bind(impl_->socket, reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) != 0) {
        const int error = last_error();
        close_socket(impl_->socket);
        impl_->socket = invalid_socket;
        throw std::system_error{error, std::system_category(), "UDP bind"};
    }
    if (!make_nonblocking(impl_->socket)) {
        const int error = last_error();
        close_socket(impl_->socket);
        impl_->socket = invalid_socket;
        throw std::system_error{error, std::system_category(), "UDP nonblocking"};
    }
    sockaddr_in bound{};
#if defined(_WIN32)
    int bound_size = sizeof(bound);
#else
    socklen_t bound_size = sizeof(bound);
#endif
    if (getsockname(impl_->socket, reinterpret_cast<sockaddr*>(&bound),
                    &bound_size) != 0) {
        const int error = last_error();
        close_socket(impl_->socket);
        impl_->socket = invalid_socket;
        throw std::system_error{error, std::system_category(), "UDP getsockname"};
    }
    impl_->endpoint.port = ntohs(bound.sin_port);
}

UdpReceiver::~UdpReceiver() {
    if (impl_ && impl_->socket != invalid_socket) close_socket(impl_->socket);
}

DatagramResult UdpReceiver::receive(std::span<std::uint8_t> buffer) noexcept {
    if (buffer.empty()) return {DatagramStatus::failed, 0, 0};
    sockaddr_in source{};
#if defined(_WIN32)
    int source_size = sizeof(source);
#else
    socklen_t source_size = sizeof(source);
#endif
    const auto received = ::recvfrom(impl_->socket, reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(std::min<std::size_t>(buffer.size(),
                                              std::numeric_limits<int>::max())),
        0, reinterpret_cast<sockaddr*>(&source), &source_size);
    if (received >= 0) {
        char source_address[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &source.sin_addr, source_address,
                      sizeof(source_address)) == nullptr) {
            return {DatagramStatus::failed, 0, last_error()};
        }
        return {DatagramStatus::received, static_cast<std::size_t>(received), 0,
                {source_address, ntohs(source.sin_port)}};
    }
    const int error = last_error();
    if (would_block(error)) return {DatagramStatus::would_block, 0, error};
    if (truncated(error)) return {DatagramStatus::truncated, buffer.size(), error};
    return {DatagramStatus::failed, 0, error};
}

const UdpEndpoint& UdpReceiver::local() const noexcept { return impl_->endpoint; }

std::size_t UdpReceiver::receive_buffer_bytes() const noexcept {
    return impl_->receive_buffer_bytes;
}

}  // namespace efis::io
