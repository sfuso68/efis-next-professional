#include "efis/core/flight_model.hpp"
#include "efis/providers/ddp_udp_publisher.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::atomic_bool running{true};
void stop(int) { running.store(false); }

struct Options final {
    std::string destination;
    std::uint16_t port{49'002};
    std::uint32_t source_node{47};
    int rate_hz{20};
    int seconds{};
    bool simulate{};
};

Options parse(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        auto value = [&](const char* name) -> std::string {
            if (++index >= argc) throw std::invalid_argument{std::string{name} + " needs a value"};
            return argv[index];
        };
        if (argument == "--destination") options.destination = value("--destination");
        else if (argument == "--port") {
            const auto port = std::stoul(value("--port"));
            if (port > 65'535U) throw std::invalid_argument{"invalid UDP port"};
            options.port = static_cast<std::uint16_t>(port);
        }
        else if (argument == "--source-node") options.source_node = std::stoul(value("--source-node"));
        else if (argument == "--rate-hz") options.rate_hz = std::stoi(value("--rate-hz"));
        else if (argument == "--seconds") options.seconds = std::stoi(value("--seconds"));
        else if (argument == "--simulate") options.simulate = true;
        else throw std::invalid_argument{"unknown argument: " + argument};
    }
    if (options.destination.empty()) throw std::invalid_argument{"--destination is required"};
    if (!options.simulate) {
        throw std::invalid_argument{"bench publisher requires explicit --simulate"};
    }
    if (options.port == 0 || options.rate_hz < 1 || options.rate_hz > 100 ||
        options.seconds < 0) {
        throw std::invalid_argument{"invalid port, rate or duration"};
    }
    return options;
}

std::uint64_t publisher_instance() {
    std::random_device random;
    const auto high = static_cast<std::uint64_t>(random()) << 32U;
    const auto low = static_cast<std::uint64_t>(random());
    const auto value = high | low;
    return value == 0 ? 1 : value;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    try {
        const auto options = parse(argc, argv);
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);

        const auto instance = publisher_instance();
        efis::DdpUdpPublisher publisher{{
            {options.destination, options.port},
            {efis::protocol::ddp::Profile::professional,
             options.source_node, instance, 1}}};
        efis::SimulationProvider simulation{42};
        efis::FlightModel model;
        const auto started = efis::MonoClock::now();
        auto next = started;
        const auto period = std::chrono::microseconds{1'000'000 / options.rate_hz};

        std::cout << "event=ddp_bench_start mode=SIM destination="
                  << options.destination << ':' << options.port
                  << " source_node=" << options.source_node
                  << " instance=" << instance << " rate_hz=" << options.rate_hz << '\n';

        while (running.load()) {
            const auto now = efis::MonoClock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - started);
            publisher.publish(model.normalize(simulation.sample(now, elapsed), now), now);
            if (options.seconds > 0 && elapsed >= std::chrono::seconds{options.seconds}) break;
            next += period;
            std::this_thread::sleep_until(next);
        }

        std::cout << "event=ddp_bench_stop datagrams="
                  << publisher.metrics().datagrams << " bytes="
                  << publisher.metrics().bytes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "event=ddp_bench_failed reason=\"" << error.what() << "\"\n";
        return 2;
    }
}
