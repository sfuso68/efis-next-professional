#include "efis/ingest/blackbox_publisher_session.hpp"
#include "efis/io/socketcan_source.hpp"
#include "efis/io/readonly_udp_fanout.hpp"
#include "efis/io/udp_datagram.hpp"
#include "efis/providers/edp_udp.hpp"
#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/recording/segmented_recorder.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::atomic_bool running{true};
void stop(int) { running.store(false); }

struct Options final {
    std::string mavlink_bind{"0.0.0.0"};
    std::uint16_t mavlink_port{14'550};
    std::string mavlink_source;
    std::vector<std::string> mavlink_observers;
    std::uint16_t mavlink_observer_port{14'550};
    std::string can_interface;
    std::vector<std::string> displays;
    std::filesystem::path records;
    std::uint16_t display_port{49'002};
    std::string engine_display;
    std::uint16_t engine_port{49'003};
    bool engine_simulation{};
    std::uint32_t source_node{47};
    int rate_hz{20};
    int seconds{};
};

template <class T>
T unsigned_value(const std::string& text, const char* name, T maximum) {
    std::size_t used{};
    const auto value = std::stoull(text, &used);
    if (used != text.size() || value == 0 || value > maximum) {
        throw std::invalid_argument{std::string{"invalid "} + name};
    }
    return static_cast<T>(value);
}

Options parse(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        auto value = [&](const char* name) -> std::string {
            if (++index >= argc) {
                throw std::invalid_argument{std::string{name} + " needs a value"};
            }
            return argv[index];
        };
        if (argument == "--mavlink-bind") options.mavlink_bind = value("--mavlink-bind");
        else if (argument == "--mavlink-port") {
            options.mavlink_port = unsigned_value<std::uint16_t>(
                value("--mavlink-port"), "MAVLink port", 65'535);
        } else if (argument == "--mavlink-source") {
            options.mavlink_source = value("--mavlink-source");
        } else if (argument == "--mavlink-observer") {
            options.mavlink_observers.push_back(value("--mavlink-observer"));
        } else if (argument == "--mavlink-observer-port") {
            options.mavlink_observer_port = unsigned_value<std::uint16_t>(
                value("--mavlink-observer-port"), "MAVLink observer port", 65'535);
        } else if (argument == "--can-interface") {
            options.can_interface = value("--can-interface");
        } else if (argument == "--display") {
            options.displays.push_back(value("--display"));
        } else if (argument == "--engine-display") {
            options.engine_display = value("--engine-display");
        } else if (argument == "--engine-port") {
            options.engine_port = unsigned_value<std::uint16_t>(
                value("--engine-port"), "engine port", 65'535);
        } else if (argument == "--engine-simulation") {
            options.engine_simulation = true;
        }
        else if (argument == "--records") options.records = value("--records");
        else if (argument == "--display-port") {
            options.display_port = unsigned_value<std::uint16_t>(
                value("--display-port"), "display port", 65'535);
        } else if (argument == "--source-node") {
            options.source_node = unsigned_value<std::uint32_t>(
                value("--source-node"), "source node",
                std::numeric_limits<std::uint32_t>::max());
        } else if (argument == "--rate-hz") {
            options.rate_hz = unsigned_value<int>(value("--rate-hz"), "rate", 100);
        } else if (argument == "--seconds") {
            options.seconds = unsigned_value<int>(value("--seconds"), "duration",
                                                  std::numeric_limits<int>::max());
        } else {
            throw std::invalid_argument{"unknown argument: " + argument};
        }
    }
    if (options.displays.empty()) {
        throw std::invalid_argument{"at least one --display is required"};
    }
    if (options.records.empty()) {
        throw std::invalid_argument{"--records is required"};
    }
    if (!options.mavlink_observers.empty()) {
        if (options.mavlink_source.empty()) {
            throw std::invalid_argument{
                "--mavlink-source is required when observers are enabled"};
        }
        if (options.mavlink_bind == "0.0.0.0") {
            throw std::invalid_argument{
                "observer mode requires an explicit MAVLink bind address"};
        }
    }
    if (!options.engine_display.empty() && !options.engine_simulation) {
        throw std::invalid_argument{
            "engine display requires an implemented engine source; use "
            "--engine-simulation only for an explicitly simulated bench feed"};
    }
    return options;
}

std::uint64_t publisher_instance() {
    std::random_device random;
    const auto value = (static_cast<std::uint64_t>(random()) << 32U) |
                       static_cast<std::uint64_t>(random());
    return value == 0 ? 1 : value;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    try {
        const auto options = parse(argc, argv);
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        std::cout << std::unitbuf;

        efis::io::UdpReceiver mavlink{{options.mavlink_bind, options.mavlink_port}};
        std::unique_ptr<efis::io::ReadOnlyUdpFanout> mavlink_observers;
        if (!options.mavlink_observers.empty()) {
            std::vector<efis::io::UdpEndpoint> destinations;
            destinations.reserve(options.mavlink_observers.size());
            for (const auto& address : options.mavlink_observers) {
                destinations.push_back({address, options.mavlink_observer_port});
            }
            mavlink_observers = std::make_unique<efis::io::ReadOnlyUdpFanout>(
                std::move(destinations), options.mavlink_source);
        }
        std::unique_ptr<efis::io::SocketCanSource> can;
        if (!options.can_interface.empty()) {
            if (!efis::io::SocketCanSource::supported()) {
                throw std::runtime_error{"SocketCAN is not supported on this platform"};
            }
            can = std::make_unique<efis::io::SocketCanSource>(options.can_interface);
            if (!can->open()) {
                throw std::runtime_error{"cannot open CAN interface: " + can->error()};
            }
        }

        const auto instance = publisher_instance();
        efis::recording::SegmentedRecorder recorder{options.records};
        std::vector<efis::DdpUdpPublisherConfig> publishers;
        publishers.reserve(options.displays.size());
        for (const auto& display : options.displays) {
            publishers.push_back({
                {display, options.display_port},
                {efis::protocol::ddp::Profile::professional,
                 options.source_node, instance, 1}});
        }
        efis::ingest::BlackboxPublisherSession session{
            std::move(publishers), {}, &recorder};
        std::unique_ptr<efis::EdpUdpPublisher> engine_publisher;
        std::unique_ptr<efis::EngineSimulationProvider> engine_simulator;
        if (!options.engine_display.empty()) {
            engine_publisher = std::make_unique<efis::EdpUdpPublisher>(
                efis::EdpUdpPublisherConfig{{options.engine_display, options.engine_port},
                                             {options.source_node, instance, 1}});
            engine_simulator = std::make_unique<efis::EngineSimulationProvider>(84);
        }

        std::array<std::uint8_t, efis::io::max_udp_payload> mavlink_buffer{};
        std::vector<efis::can::Frame> can_buffer(64);
        std::uint64_t mavlink_datagrams{};
        std::uint64_t mavlink_socket_errors{};
        std::uint64_t mavlink_truncated{};
        std::uint64_t mavlink_source_rejected{};

        const auto started = efis::MonoClock::now();
        auto next = started;
        auto next_report = started;
        const auto period = std::chrono::microseconds{1'000'000 / options.rate_hz};

        std::cout << "event=blackbox_broker_start mode=LIVE_READ_ONLY mavlink_bind="
                  << options.mavlink_bind << ':' << options.mavlink_port
                  << " mavlink_source="
                  << (options.mavlink_source.empty() ? "ANY" : options.mavlink_source)
                  << " mavlink_observer_mode=TX_ONLY mavlink_observers="
                  << options.mavlink_observers.size()
                  << " mavlink_observer_port=" << options.mavlink_observer_port
                  << " can=" << (options.can_interface.empty() ? "DISABLED" : options.can_interface)
                  << " displays=" << options.displays.size()
                  << " display_port=" << options.display_port
                  << " engine_display=" << (options.engine_display.empty() ? "DISABLED" : options.engine_display)
                  << " engine_port=" << options.engine_port
                  << " engine_source=" << (engine_simulator ? "SIMULATION" : "DISABLED")
                  << " records=" << options.records.string()
                  << " source_node=" << options.source_node
                  << " instance=" << instance << " rate_hz=" << options.rate_hz << '\n';

        while (running.load()) {
            const auto now = efis::MonoClock::now();
            for (int drained = 0; drained < 64; ++drained) {
                const auto result = mavlink.receive(mavlink_buffer);
                if (result.status == efis::io::DatagramStatus::would_block) break;
                if (result.status == efis::io::DatagramStatus::truncated) {
                    ++mavlink_truncated;
                    continue;
                }
                if (result.status == efis::io::DatagramStatus::failed) {
                    ++mavlink_socket_errors;
                    break;
                }
                if (!options.mavlink_source.empty() &&
                    result.source.address != options.mavlink_source) {
                    ++mavlink_source_rejected;
                    continue;
                }
                ++mavlink_datagrams;
                const auto datagram = std::span<const std::uint8_t>{
                    mavlink_buffer.data(), result.size};
                if (mavlink_observers) mavlink_observers->forward(datagram);
                session.consume_mavlink(datagram, now);
            }
            if (can) {
                const auto count = can->read(can_buffer);
                if (count > 0) {
                    session.consume_can(
                        std::span<const efis::can::Frame>{can_buffer.data(), count}, now);
                }
            }

            session.publish(now);
            if (engine_publisher && engine_simulator) {
                engine_publisher->publish(
                    engine_simulator->sample(now, std::chrono::duration_cast<std::chrono::milliseconds>(now - started)), now);
            }
            if (now >= next_report) {
                next_report = now + 1s;
                const auto& stats = session.statistics();
                const auto transmitted = session.publisher_metrics();
                std::cout << "event=blackbox_broker_health mavlink_datagrams="
                          << mavlink_datagrams << " mavlink_bytes=" << stats.mavlink_bytes
                          << " mavlink_socket_errors=" << mavlink_socket_errors
                          << " mavlink_truncated=" << mavlink_truncated
                          << " mavlink_source_rejected=" << mavlink_source_rejected
                          << " can_frames=" << stats.can_frames
                          << " node_events=" << stats.node_events_observed
                          << " source_events=" << stats.source_events_observed
                          << " recorded=" << stats.snapshots_recorded
                          << " ddp_datagrams=" << transmitted.datagrams
                          << " ddp_bytes=" << transmitted.bytes
                          << " ddp_failures=" << stats.ddp_publish_failures << '\n';
                if (mavlink_observers) {
                    const auto observer_stats = mavlink_observers->statistics();
                    std::cout << "event=blackbox_mavlink_observer_health mode=TX_ONLY"
                              << " datagrams=" << observer_stats.datagrams
                              << " bytes=" << observer_stats.bytes
                              << " failures=" << observer_stats.failures << '\n';
                    for (const auto& observer :
                         mavlink_observers->target_statistics()) {
                        std::cout << "event=blackbox_mavlink_observer_target destination="
                                  << observer.destination.address << ':'
                                  << observer.destination.port
                                  << " datagrams=" << observer.datagrams
                                  << " bytes=" << observer.bytes
                                  << " failures=" << observer.failures;
                        if (!observer.last_error.empty()) {
                            std::cout << " last_error=\"" << observer.last_error << '"';
                        }
                        std::cout << '\n';
                    }
                }
                if (engine_publisher) {
                    std::cout << "event=blackbox_engine_health destination="
                              << options.engine_display << ':' << options.engine_port
                              << " source=SIMULATION edp_datagrams="
                              << engine_publisher->datagrams() << " edp_bytes="
                              << engine_publisher->bytes() << '\n';
                }
                for (const auto& display : session.display_statistics()) {
                    std::cout << "event=blackbox_display_health destination="
                              << display.destination.address << ':'
                              << display.destination.port
                              << " datagrams=" << display.transmitted.datagrams
                              << " bytes=" << display.transmitted.bytes
                              << " failures=" << display.failures;
                    if (!display.last_error.empty()) {
                        std::cout << " last_error=\"" << display.last_error << '"';
                    }
                    std::cout << '\n';
                }
            }
            if (options.seconds > 0 &&
                now - started >= std::chrono::seconds{options.seconds}) break;
            next += period;
            std::this_thread::sleep_until(next);
        }

        recorder.close();
        const auto transmitted = session.publisher_metrics();
        const auto observer_stats = mavlink_observers
            ? mavlink_observers->statistics()
            : efis::io::ReadOnlyUdpFanoutStatistics{};
        std::cout << "event=blackbox_broker_stop ddp_datagrams="
                  << transmitted.datagrams
                  << " ddp_failures=" << session.statistics().ddp_publish_failures
                  << " mavlink_observer_datagrams=" << observer_stats.datagrams
                  << " mavlink_observer_failures=" << observer_stats.failures
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "event=blackbox_broker_failed reason=\"" << error.what() << "\"\n";
        return 2;
    }
}
