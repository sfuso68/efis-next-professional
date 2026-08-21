#pragma once

#include "efis/core/node_health.hpp"
#include "efis/engine/engine_snapshot.hpp"
#include "efis/io/can_source.hpp"

#include <cstdint>
#include <vector>

namespace efis {

// Identificativi dei messaggi di Engine CAN V1.
namespace engine_can {
inline constexpr std::uint8_t engine_primary = 0x01;
inline constexpr std::uint8_t engine_fluids = 0x02;
inline constexpr std::uint8_t engine_fuel = 0x03;
inline constexpr std::uint8_t engine_electrical = 0x04;
inline constexpr std::uint8_t cht_1_2 = 0x05;
inline constexpr std::uint8_t cht_3_4 = 0x06;
inline constexpr std::uint8_t egt_1_2 = 0x07;
inline constexpr std::uint8_t egt_3_4 = 0x08;
inline constexpr std::uint8_t node_heartbeat = 0x7F;

// Un sensore che il nodo non riesce a leggere trasmette questi valori. Non sono
// misure fuori scala: sono l'assenza di misura, e l'adapter le traduce in
// indisponibilita' invece che in uno zero.
inline constexpr std::uint16_t unsigned_unavailable = 0xFFFF;
inline constexpr std::int16_t signed_unavailable = -32768;

// Flag del battito.
inline constexpr std::uint8_t flag_calibration_valid = 0x01;
inline constexpr std::uint8_t flag_probe_failure = 0x02;
inline constexpr std::uint8_t flag_supply_out_of_tolerance = 0x04;
inline constexpr std::uint8_t flag_recently_restarted = 0x08;
}  // namespace engine_can

struct EngineCanStatistics final {
    std::uint64_t frames_applied{};

    // Frame di un messaggio non in tabella. Non e' un errore: e' un nodo che
    // dice piu' di quanto ci serva.
    std::uint64_t frames_ignored{};

    // Frame la cui lunghezza non e' quella del messaggio, in difetto o in
    // eccesso. A differenza di MAVLink qui non esiste il taglio degli zeri
    // finali: la lunghezza e' fissa.
    //
    // Il controllo in eccesso non e' pedanteria. Su un bus condiviso capita
    // traffico di altri protocolli che, per come e' composto l'identificatore,
    // ricade su un nodo e un messaggio validi: un CANopen 0x601 da otto byte si
    // legge come ENGINE_PRIMARY del nodo 12. Accettarlo significherebbe mostrare
    // un regime motore inventato. La lunghezza e' cio' che li distingue.
    std::uint64_t frames_wrong_length{};

    // Frame con nodo zero. Lo zero significa "non dichiarato" in tutto il
    // progetto: un nodo che lo usa e' mal configurato e va visto, non accolto.
    std::uint64_t frames_undeclared_node{};

    // Campi che il nodo ha dichiarato illeggibili.
    std::uint64_t unavailable_readings{};

    std::uint64_t sequence_wraps{};
};

// Stato che il nodo motore dichiara di se stesso, oltre alle misure.
struct EngineNodeStatus final {
    bool seen{};
    bool calibration_valid{};
    bool probe_failure{};
    bool supply_out_of_tolerance{};
    bool recently_restarted{};
    std::uint32_t uptime_s{};
};

// Traduce i frame del bus motore in campioni con provenienza completa.
//
// Adapter di sola lettura, come quello MAVLink: consuma frame e non ne produce.
// Il nodo motore misura e trasmette, e nulla in Engine CAN V1 definisce un
// canale di comando.
class EngineCanAdapter final {
public:
    explicit EngineCanAdapter(NodeHealthPolicy policy = {});

    // Consuma i frame disponibili dalla sorgente.
    void consume(io::CanSource& source, MonoTime now);

    // Applica un singolo frame. Esposta perche' una sorgente dal vivo consegna
    // un frame per volta e non ha una fine da attendere.
    void apply(const io::CanFrame& frame, MonoTime now);

    [[nodiscard]] const engine::EngineSnapshot& snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] const EngineCanStatistics& statistics() const noexcept {
        return statistics_;
    }
    [[nodiscard]] const EngineNodeStatus& node_status() const noexcept {
        return node_status_;
    }
    [[nodiscard]] const NodeHealthMonitor& health() const noexcept { return health_; }

    // Applica i timeout ai nodi. Va chiamata anche quando non arriva nulla: e'
    // il silenzio a dover produrre un evento.
    void update(MonoTime now);

    [[nodiscard]] const std::vector<NodeStateChange>& node_events() const noexcept {
        return node_events_;
    }
    void clear_node_events() noexcept { node_events_.clear(); }

private:
    // Estensione a 64 bit del contatore di sequenza, per coppia nodo e
    // messaggio: ogni messaggio ha un contatore proprio, e mescolarli
    // produrrebbe regressioni continue.
    struct SequenceTracker final {
        std::uint16_t key{};
        std::uint8_t last{};
        std::uint64_t extended{};
    };

    [[nodiscard]] std::uint64_t track(const io::CanFrame& frame);

    NodeHealthMonitor health_;
    engine::EngineSnapshot snapshot_{};
    EngineCanStatistics statistics_{};
    EngineNodeStatus node_status_{};
    std::vector<NodeStateChange> node_events_;
    std::vector<SequenceTracker> sequences_;
};

}  // namespace efis
