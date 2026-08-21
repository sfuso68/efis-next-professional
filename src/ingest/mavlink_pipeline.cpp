#include "efis/ingest/mavlink_pipeline.hpp"

#include <array>

namespace efis::ingest {

MavlinkPipeline::MavlinkPipeline(PipelinePolicy policy)
    : policy_(policy),
      adapter_(policy.nodes),
      arbitrator_(policy.arbitration),
      model_(policy.model) {}

void MavlinkPipeline::consume(std::span<const std::uint8_t> bytes, MonoTime now) {
    adapter_.consume(bytes, now);
    recompute(now);
}

void MavlinkPipeline::update(MonoTime now) {
    // I timeout dei nodi vanno applicati prima di ricalcolare lo stato: un nodo
    // appena dichiarato perduto non deve contribuire con i suoi ultimi valori
    // come se fosse ancora presente.
    adapter_.update(now);
    recompute(now);
}

void MavlinkPipeline::recompute(MonoTime now) {
    const std::array<SnapshotContribution, 1> contributions{
        SnapshotContribution{policy_.source, adapter_.snapshot()}};

    // L'arbitrato con una sola sorgente non sceglie niente, e va eseguito lo
    // stesso. Non e' cerimonia: e' il punto in cui la scelta sara' fatta quando
    // le sorgenti saranno due, ed e' gia' da ora il posto che risponde alla
    // domanda "da dove viene questo numero". Tenerlo fuori finche' non serve
    // significherebbe doverlo inserire piu' tardi in mezzo a un percorso che nel
    // frattempo si e' consolidato senza.
    FlightSnapshot chosen = arbitrator_.combine(contributions, now);

    // La normalizzazione porta con se' invecchiamento, sorveglianza del valore
    // fermo, confronto fra catene indipendenti, salute complessiva e allarmi.
    // E' qui che i tre controlli diventano effettivi sui dati del Pixhawk.
    state_ = model_.normalize(std::move(chosen), now);
}

void MavlinkPipeline::clear_events() noexcept {
    adapter_.clear_node_events();
    arbitrator_.clear_events();
}

}  // namespace efis::ingest
