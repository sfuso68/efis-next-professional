#pragma once

#include "efis/core/flight_source.hpp"
#include "efis/core/source_arbitrator.hpp"
#include "efis/engine/engine_snapshot.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace efis {

// Una sorgente di stato motore.
//
// Vale qui la stessa regola del volo: non dichiara da dove prenda i dati. Puo'
// essere la simulazione, il nodo sensori su CAN, o il collegamento verso il nodo
// di ingestion. Il quadro che la consuma non deve saperlo.
class EngineSource {
public:
    EngineSource() = default;
    EngineSource(const EngineSource&) = delete;
    EngineSource& operator=(const EngineSource&) = delete;
    EngineSource(EngineSource&&) = delete;
    EngineSource& operator=(EngineSource&&) = delete;
    virtual ~EngineSource() = default;

    [[nodiscard]] virtual SourceId source() const noexcept = 0;
    [[nodiscard]] virtual engine::EngineSnapshot poll(
        MonoTime now, std::chrono::milliseconds elapsed) = 0;
};

struct EngineContribution final {
    SourceId source{SourceId::unknown};
    engine::EngineSnapshot snapshot{};
};

// Arbitra fra piu' sorgenti motore e produce un solo stato.
//
// Per grandezza, non per sorgente, esattamente come per lo stato di volo: se il
// nodo sensori perde il canale della pressione olio ma continua a misurare i
// giri, si sostituisce la pressione e si tengono i giri veri. Il motore ha
// diciotto grandezze su canali fisicamente distinti, quindi il guasto parziale
// qui e' la norma piu' che l'eccezione.
class EngineArbitrator final {
public:
    explicit EngineArbitrator(ArbitrationPolicy policy = {});

    [[nodiscard]] engine::EngineSnapshot combine(
        std::span<const EngineContribution> contributions, MonoTime now);

    [[nodiscard]] const std::vector<SourceChangeEvent>& events() const noexcept {
        return events_;
    }
    void clear_events() noexcept { events_.clear(); }

    [[nodiscard]] const ArbitrationStatistics& statistics() const noexcept {
        return statistics_;
    }

    [[nodiscard]] SourceId selected_for(SignalId signal) const noexcept;

    void reset() noexcept;

private:
    ArbitrationPolicy policy_;
    std::vector<SourceArbitrator<double>> arbitrators_;
    std::vector<SourceChangeEvent> events_;
    ArbitrationStatistics statistics_{};
    std::uint64_t sequence_{};
};

// Interroga le sorgenti motore registrate e produce un solo stato arbitrato.
//
// Il ruolo della simulazione e' lo stesso concetto gia' usato per il volo, e non
// e' un caso: al banco motore la condizione normale e' avere qualche canale
// collegato e molti no.
class EngineSourceRouter final {
public:
    explicit EngineSourceRouter(ArbitrationPolicy policy = {});

    void add_source(std::shared_ptr<EngineSource> source);
    void set_simulation(std::shared_ptr<EngineSource> simulation);

    void set_simulation_role(SimulationRole role) noexcept {
        if (role == role_) return;
        role_ = role;
        arbitrator_.reset();
    }
    [[nodiscard]] SimulationRole simulation_role() const noexcept { return role_; }

    [[nodiscard]] engine::EngineSnapshot poll(MonoTime now,
                                              std::chrono::milliseconds elapsed);

    [[nodiscard]] const EngineArbitrator& arbitrator() const noexcept {
        return arbitrator_;
    }
    [[nodiscard]] std::size_t source_count() const noexcept { return sources_.size(); }

private:
    EngineArbitrator arbitrator_;
    std::vector<std::shared_ptr<EngineSource>> sources_;
    std::shared_ptr<EngineSource> simulation_;
    SimulationRole role_{SimulationRole::fallback};
    engine::EngineSnapshot simulated_{};
    std::vector<EngineContribution> contributions_;
};

}  // namespace efis
