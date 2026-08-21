#include "efis/ingest/mavlink_pipeline.hpp"

#include <array>

namespace efis::ingest {

MavlinkPipeline::MavlinkPipeline(PipelinePolicy policy)
    : policy_(policy),
      adapter_(policy.nodes),
      esp32_adapter_(policy.esp32_node, policy.nodes),
      arbitrator_(policy.arbitration),
      model_(policy.model) {}

void MavlinkPipeline::consume(std::span<const std::uint8_t> bytes, MonoTime now) {
    adapter_.consume(bytes, now);
    recompute(now);
}

void MavlinkPipeline::consume_can(std::span<const can::Frame> frames, MonoTime now) {
    esp32_adapter_.consume(frames, now);
    recompute(now);
}

void MavlinkPipeline::consume_can(const can::Frame& frame, MonoTime now) {
    esp32_adapter_.consume(frame, now);
    recompute(now);
}

void MavlinkPipeline::update(MonoTime now) {
    // I timeout dei nodi vanno applicati prima di ricalcolare lo stato: un nodo
    // appena dichiarato perduto non deve contribuire con i suoi ultimi valori
    // come se fosse ancora presente.
    adapter_.update(now);
    esp32_adapter_.update(now);
    recompute(now);
}

void MavlinkPipeline::recompute(MonoTime now) {
    const std::array<SnapshotContribution, 2> contributions{
        SnapshotContribution{policy_.source, adapter_.snapshot()},
        SnapshotContribution{policy_.esp32_source, esp32_adapter_.snapshot()}};

    // L'ordine dichiara la preferenza: Pixhawk prima, nodo ESP32 poi. La scelta
    // resta per singola grandezza, quindi un guasto del pitot non trascina con
    // se' assetto, quota o gli altri dati ancora sani della sorgente primaria.
    FlightSnapshot chosen = arbitrator_.combine(contributions, now);

    // La normalizzazione porta con se' invecchiamento, sorveglianza del valore
    // fermo, confronto fra catene indipendenti, salute complessiva e allarmi.
    // E' qui che i tre controlli diventano effettivi sui dati del Pixhawk.
    state_ = model_.normalize(std::move(chosen), now);
}

std::vector<NodeStateChange> MavlinkPipeline::node_events() const {
    std::vector<NodeStateChange> events;
    events.reserve(adapter_.node_events().size() + esp32_adapter_.node_events().size());
    events.insert(events.end(), adapter_.node_events().begin(), adapter_.node_events().end());
    events.insert(events.end(), esp32_adapter_.node_events().begin(),
                  esp32_adapter_.node_events().end());
    return events;
}

void MavlinkPipeline::clear_events() noexcept {
    adapter_.clear_node_events();
    esp32_adapter_.clear_node_events();
    arbitrator_.clear_events();
}

}  // namespace efis::ingest
