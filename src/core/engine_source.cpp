#include "efis/core/engine_source.hpp"

#include <algorithm>

namespace efis {
namespace {

// Tabella delle grandezze motore arbitrate, con la stessa forma di quella dello
// stato di volo: un segnale, il campo che lo contiene in lettura e in scrittura.
struct EngineFieldAccess final {
    SignalId signal;
    const Sample<double>& (*read)(const engine::EngineSnapshot&);
    Sample<double>& (*write)(engine::EngineSnapshot&);
};

using Snapshot = engine::EngineSnapshot;

constexpr EngineFieldAccess engine_fields[] = {
    {SignalId::engine_rpm,
     [](const Snapshot& s) -> const Sample<double>& { return s.rpm; },
     [](Snapshot& s) -> Sample<double>& { return s.rpm; }},
    {SignalId::manifold_pressure,
     [](const Snapshot& s) -> const Sample<double>& { return s.manifold_pressure_inhg; },
     [](Snapshot& s) -> Sample<double>& { return s.manifold_pressure_inhg; }},
    {SignalId::oil_pressure,
     [](const Snapshot& s) -> const Sample<double>& { return s.oil_pressure_psi; },
     [](Snapshot& s) -> Sample<double>& { return s.oil_pressure_psi; }},
    {SignalId::oil_temperature,
     [](const Snapshot& s) -> const Sample<double>& { return s.oil_temperature_c; },
     [](Snapshot& s) -> Sample<double>& { return s.oil_temperature_c; }},
    {SignalId::fuel_pressure,
     [](const Snapshot& s) -> const Sample<double>& { return s.fuel_pressure_psi; },
     [](Snapshot& s) -> Sample<double>& { return s.fuel_pressure_psi; }},
    {SignalId::fuel_flow,
     [](const Snapshot& s) -> const Sample<double>& { return s.fuel_flow_gph; },
     [](Snapshot& s) -> Sample<double>& { return s.fuel_flow_gph; }},
    {SignalId::fuel_quantity_left,
     [](const Snapshot& s) -> const Sample<double>& { return s.fuel_left_gal; },
     [](Snapshot& s) -> Sample<double>& { return s.fuel_left_gal; }},
    {SignalId::fuel_quantity_right,
     [](const Snapshot& s) -> const Sample<double>& { return s.fuel_right_gal; },
     [](Snapshot& s) -> Sample<double>& { return s.fuel_right_gal; }},
    {SignalId::bus_voltage,
     [](const Snapshot& s) -> const Sample<double>& { return s.bus_voltage_v; },
     [](Snapshot& s) -> Sample<double>& { return s.bus_voltage_v; }},
    {SignalId::alternator_current,
     [](const Snapshot& s) -> const Sample<double>& { return s.alternator_current_a; },
     [](Snapshot& s) -> Sample<double>& { return s.alternator_current_a; }},

    // I quattro cilindri restano quattro grandezze distinte, in testa e allo
    // scarico. Un cilindro che perde la sonda non deve trascinarsi dietro gli
    // altri tre, che stanno misurando benissimo.
    {SignalId::cylinder_head_temperature_1,
     [](const Snapshot& s) -> const Sample<double>& { return s.cht_c[0]; },
     [](Snapshot& s) -> Sample<double>& { return s.cht_c[0]; }},
    {SignalId::cylinder_head_temperature_2,
     [](const Snapshot& s) -> const Sample<double>& { return s.cht_c[1]; },
     [](Snapshot& s) -> Sample<double>& { return s.cht_c[1]; }},
    {SignalId::cylinder_head_temperature_3,
     [](const Snapshot& s) -> const Sample<double>& { return s.cht_c[2]; },
     [](Snapshot& s) -> Sample<double>& { return s.cht_c[2]; }},
    {SignalId::cylinder_head_temperature_4,
     [](const Snapshot& s) -> const Sample<double>& { return s.cht_c[3]; },
     [](Snapshot& s) -> Sample<double>& { return s.cht_c[3]; }},
    {SignalId::exhaust_gas_temperature_1,
     [](const Snapshot& s) -> const Sample<double>& { return s.egt_c[0]; },
     [](Snapshot& s) -> Sample<double>& { return s.egt_c[0]; }},
    {SignalId::exhaust_gas_temperature_2,
     [](const Snapshot& s) -> const Sample<double>& { return s.egt_c[1]; },
     [](Snapshot& s) -> Sample<double>& { return s.egt_c[1]; }},
    {SignalId::exhaust_gas_temperature_3,
     [](const Snapshot& s) -> const Sample<double>& { return s.egt_c[2]; },
     [](Snapshot& s) -> Sample<double>& { return s.egt_c[2]; }},
    {SignalId::exhaust_gas_temperature_4,
     [](const Snapshot& s) -> const Sample<double>& { return s.egt_c[3]; },
     [](Snapshot& s) -> Sample<double>& { return s.egt_c[3]; }},
};

constexpr std::size_t engine_field_count = std::size(engine_fields);

}  // namespace

EngineArbitrator::EngineArbitrator(ArbitrationPolicy policy) : policy_(policy) {
    arbitrators_.reserve(engine_field_count);
    for (std::size_t i = 0; i < engine_field_count; ++i) arbitrators_.emplace_back(policy_);
}

void EngineArbitrator::reset() noexcept {
    for (auto& arbitrator : arbitrators_) arbitrator = SourceArbitrator<double>{policy_};
}

engine::EngineSnapshot EngineArbitrator::combine(
    std::span<const EngineContribution> contributions, MonoTime now) {
    engine::EngineSnapshot out{};

    std::vector<Sample<double>> candidates;
    candidates.reserve(contributions.size());
    for (std::size_t field = 0; field < engine_field_count; ++field) {
        const auto& access = engine_fields[field];
        candidates.clear();
        for (const EngineContribution& contribution : contributions) {
            const Sample<double>& candidate = access.read(contribution.snapshot);
            if (candidate.usable() &&
                candidate.origin.signal != SignalId::unknown &&
                candidate.origin.signal != access.signal) {
                ++statistics_.mislabelled_samples;
            }
            candidates.push_back(candidate);
        }
        auto result = arbitrators_[field].select(candidates, now);
        access.write(out) = result.selected;
        if (result.change.has_value()) {
            events_.push_back(*result.change);
            switch (result.change->reason) {
                case SourceChangeReason::failover: ++statistics_.failovers; break;
                case SourceChangeReason::reversion: ++statistics_.reversions; break;
                case SourceChangeReason::initial: break;
            }
        }
    }

    // Salute e guasto iniettato non si arbitrano: descrivono la sorgente che li
    // ha prodotti, non una grandezza misurata. Si prendono dal primo contributo,
    // cioe' da chi ha effettivamente la parola sul motore.
    if (!contributions.empty()) {
        out.health = contributions.front().snapshot.health;
        out.injected_fault = contributions.front().snapshot.injected_fault;
    }

    // Nessun dato non e' "normale". Senza contributi la salute resterebbe al
    // valore predefinito, e il quadro direbbe che il motore sta bene mentre non
    // sta misurando niente: e' il dato plausibile ma falso che il paragrafo 22
    // dell'handoff vieta, nella forma piu' pericolosa, perche' rassicura.
    const bool measured = out.rpm.usable() || out.oil_pressure_psi.usable() ||
                          out.cht_c[0].usable();
    if (!measured) out.health = engine::EngineHealth::warning;
    out.timestamp = now;
    out.sequence = ++sequence_;
    return out;
}

SourceId EngineArbitrator::selected_for(SignalId signal) const noexcept {
    for (std::size_t field = 0; field < engine_field_count; ++field) {
        if (engine_fields[field].signal == signal) {
            return arbitrators_[field].selected_source();
        }
    }
    return SourceId::unknown;
}

// --- Router ------------------------------------------------------------------

EngineSourceRouter::EngineSourceRouter(ArbitrationPolicy policy)
    : arbitrator_(policy) {}

void EngineSourceRouter::add_source(std::shared_ptr<EngineSource> source) {
    if (source != nullptr) sources_.push_back(std::move(source));
}

void EngineSourceRouter::set_simulation(std::shared_ptr<EngineSource> simulation) {
    simulation_ = std::move(simulation);
}

engine::EngineSnapshot EngineSourceRouter::poll(MonoTime now,
                                                std::chrono::milliseconds elapsed) {
    contributions_.clear();

    // Come per il volo, la simulazione viene interrogata anche da disabilitata
    // perche' lo scenario deve continuare ad avanzare: al banco motore un salto
    // dei giri alla riaccensione si scambia per un guasto.
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
