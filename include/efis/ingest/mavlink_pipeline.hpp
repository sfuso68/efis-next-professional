#pragma once

#include "efis/core/flight_model.hpp"
#include "efis/core/flight_snapshot.hpp"
#include "efis/core/node_health.hpp"
#include "efis/core/snapshot_arbitrator.hpp"
#include "efis/providers/can_frame.hpp"
#include "efis/providers/esp32_can_adapter.hpp"
#include "efis/providers/mavlink_adapter.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace efis::ingest {

// Compone la catena che porta dai byte del Pixhawk allo stato di volo protetto.
//
// I pezzi esistevano gia' tutti e nessuno li teneva insieme: l'adapter traduceva
// i frame in campioni, e arbitrato, sorveglianza della stasi e confronto
// incrociato restavano moduli corretti fuori dal percorso dei dati. Un controllo
// che non sta sul percorso non protegge nulla, e la sua esistenza rischia di
// essere scambiata per copertura.
//
// La catena e' quella stabilita dal progetto, non una scelta di questo file:
//
//   byte -> MavlinkAdapter -> SnapshotArbitrator -> FlightModel -> stato
//
// L'arbitro precede la normalizzazione perche' sceglie fra sorgenti e non
// invecchia nulla; il modello di volo invecchia, sorveglia e allarma su cio' che
// e' stato scelto.
//
// Perche' qui e non dentro l'adapter. La ADR-003 stabilisce che gli adapter
// producono campioni e non li scelgono ne' li mostrano: mettere l'arbitrato
// dentro il traduttore MAVLink lo renderebbe anche l'arbitro di sorgenti che non
// conosce, e il giorno in cui arrivano ESP32-S3 e SocketCAN andrebbe disfatto.
// Questa classe e' il punto di composizione, e appartiene a un livello che
// dipende dal core e dagli adapter mentre nessuno dei due dipende da lei.
class MavlinkPipeline;

struct PipelinePolicy final {
    NodeHealthPolicy nodes{};
    FlightModelPolicy model{};
    ArbitrationPolicy arbitration{};

    // Identita' della sorgente attribuita a questo adapter nell'arbitrato.
    // Quando si aggiungeranno altri bus, ciascuno portera' la propria.
    SourceId source{SourceId::pixhawk_primary};

    // Seconda catena indipendente: sensori, alimentazione, bus e firmware non
    // sono quelli del Pixhawk. Rimane una sorgente di sola lettura dell'EFIS.
    SourceId esp32_source{SourceId::esp32_legacy};
    NodeId esp32_node{21};
};

class MavlinkPipeline final {
public:
    explicit MavlinkPipeline(PipelinePolicy policy = {});

    // Consuma byte grezzi e ricalcola lo stato protetto.
    void consume(std::span<const std::uint8_t> bytes, MonoTime now);

    // Consuma frame gia' letti dal trasporto SocketCAN. Il trasporto resta
    // fuori dalla pipeline e dal core: qui entrano frame normalizzati, non
    // file descriptor Linux.
    void consume_can(std::span<const can::Frame> frames, MonoTime now);
    void consume_can(const can::Frame& frame, MonoTime now);

    // Fa avanzare il tempo senza nuovi dati.
    //
    // Non e' una comodita': scadenza, stasi e disaccordo sono grandezze che si
    // misurano nel tempo, quindi senza questa chiamata un collegamento che
    // ammutolisce resterebbe indefinitamente all'ultimo stato buono. Va chiamata
    // a ogni ciclo, anche quando non arriva niente.
    void update(MonoTime now);

    // Stato di volo dopo arbitrato, invecchiamento, sorveglianza della stasi,
    // confronto incrociato, salute e allarmi. E' l'unico che i consumatori
    // devono usare: lo stato grezzo dell'adapter resta accessibile per la
    // diagnostica, non per essere mostrato.
    [[nodiscard]] const FlightSnapshot& state() const noexcept { return state_; }

    [[nodiscard]] const MavlinkAdapter& adapter() const noexcept { return adapter_; }
    [[nodiscard]] const Esp32CanAdapter& esp32_adapter() const noexcept {
        return esp32_adapter_;
    }
    [[nodiscard]] const FlightModel& model() const noexcept { return model_; }
    [[nodiscard]] const SnapshotArbitrator& arbitrator() const noexcept {
        return arbitrator_;
    }

    // Sorgente scelta per una grandezza. Con un solo bus risponde sempre lo
    // stesso, ed e' comunque la domanda giusta da poter fare: il giorno in cui
    // le sorgenti sono due, questa e' l'unica risposta che dice da dove viene
    // davvero il numero mostrato.
    [[nodiscard]] SourceId selected_for(SignalId signal) const noexcept {
        return arbitrator_.selected_for(signal);
    }

    // Eventi destinati alla black box: transizioni di stato dei nodi e cambi di
    // sorgente, raccolti in un punto solo perche' chi registra non debba sapere
    // da quale pezzo della catena provengano.
    [[nodiscard]] std::vector<NodeStateChange> node_events() const;
    [[nodiscard]] const std::vector<SourceChangeEvent>& source_events() const noexcept {
        return arbitrator_.events();
    }
    void clear_events() noexcept;

private:
    void recompute(MonoTime now);

    PipelinePolicy policy_;
    MavlinkAdapter adapter_;
    Esp32CanAdapter esp32_adapter_;
    SnapshotArbitrator arbitrator_;
    FlightModel model_;
    FlightSnapshot state_{};
};

}  // namespace efis::ingest
