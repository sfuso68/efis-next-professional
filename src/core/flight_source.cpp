#include "efis/core/flight_source.hpp"

namespace efis {

FlightSourceRouter::FlightSourceRouter(ArbitrationPolicy policy)
    : arbitrator_(policy) {}

void FlightSourceRouter::add_source(std::shared_ptr<FlightSource> source) {
    if (source != nullptr) sources_.push_back(std::move(source));
}

void FlightSourceRouter::set_simulation(std::shared_ptr<FlightSource> simulation) {
    simulation_ = std::move(simulation);
}

FlightSnapshot FlightSourceRouter::poll(MonoTime now, std::chrono::milliseconds elapsed) {
    contributions_.clear();

    // La simulazione viene interrogata anche quando e' disabilitata, ma il suo
    // contributo non entra nell'arbitrato. Interrogarla comunque tiene avanzato
    // lo scenario: riabilitandola non riparte da dove era stata spenta, con un
    // salto che al banco si scambierebbe per un guasto.
    const bool simulate = simulation_ != nullptr;
    if (simulate) simulated_ = simulation_->poll(now, elapsed);

    if (simulate && role_ == SimulationRole::primary) {
        contributions_.push_back({simulation_->source(), simulated_});
    }

    for (const auto& source : sources_) {
        contributions_.push_back({source->source(), source->poll(now, elapsed)});
    }

    if (simulate && role_ == SimulationRole::fallback) {
        contributions_.push_back({simulation_->source(), simulated_});
    }

    return arbitrator_.combine(contributions_, now);
}

}  // namespace efis
