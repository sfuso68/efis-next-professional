#pragma once

#include "efis/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace efis {

// Stato di un nodo che produce dati: Pixhawk, modulo motore ESP32-S3, nodo black
// box. Vedi paragrafi 6 e 17.1 dell'handoff.
enum class NodeState : std::uint8_t {
    // Nodo dichiarato in configurazione ma mai sentito da questo avvio.
    //
    // Non e' `lost`: non si puo' dichiarare perduto qualcosa che non si e' mai
    // avuto. La distinzione e' operativa, non formale. "Mai sentito" indica un
    // cablaggio assente, un nodo spento o una configurazione sbagliata; "perso"
    // indica un guasto sopravvenuto in volo. Le due condizioni richiedono azioni
    // diverse e non devono essere confuse in una sola spia.
    unknown,

    // Battito ricevuto entro il tempo previsto.
    online,

    // Nessun battito entro stale_after. Il nodo potrebbe tornare, ma i suoi dati
    // non possono piu' essere presentati come attuali.
    stale,

    // Nessun battito entro lost_after. Il nodo e' considerato perduto.
    lost
};

[[nodiscard]] constexpr const char* node_state_name(NodeState state) noexcept {
    switch (state) {
        case NodeState::unknown: return "UNKNOWN";
        case NodeState::online: return "ONLINE";
        case NodeState::stale: return "STALE";
        case NodeState::lost: return "LOST";
    }
    return "UNKNOWN";
}

// Un nodo con dati non attuali non e' utilizzabile, esattamente come un campione
// scaduto. Serve a impedire che un consumer tratti come valido cio' che proviene
// da un nodo silenzioso.
[[nodiscard]] constexpr bool is_usable(NodeState state) noexcept {
    return state == NodeState::online;
}

// Esito della valutazione di un battito ricevuto.
//
// Regola 2 del Display Data Protocol V2: sequenze duplicate o arretrate vengono
// registrate e scartate. Sono qui distinte perche' descrivono guasti diversi.
enum class HeartbeatVerdict : std::uint8_t {
    // Sequenza successiva a quella attesa: battito valido.
    accepted,

    // Stessa sequenza gia' ricevuta. Tipico di un nodo bloccato che ripete
    // l'ultimo messaggio, o di un duplicato introdotto dal trasporto.
    duplicate,

    // Sequenza inferiore all'ultima ricevuta. Indica un riavvio non dichiarato
    // del nodo, un secondo trasmettitore che usa lo stesso identificativo, o un
    // riordino dei pacchetti.
    regressed,

    // Battito valido, ma con salto rispetto alla sequenza attesa: alcuni
    // messaggi non sono arrivati. Accettato, e registrato come perdita.
    accepted_after_gap
};

[[nodiscard]] constexpr bool accepts_data(HeartbeatVerdict verdict) noexcept {
    return verdict == HeartbeatVerdict::accepted ||
           verdict == HeartbeatVerdict::accepted_after_gap;
}

struct NodeHealthPolicy final {
    // Oltre questo silenzio i dati del nodo non sono piu' attuali.
    std::chrono::milliseconds stale_after{1000};

    // Oltre questo silenzio il nodo e' considerato perduto.
    std::chrono::milliseconds lost_after{3000};
};

struct NodeStatus final {
    NodeId node{node_undeclared};
    NodeState state{NodeState::unknown};

    // Istante dell'ultimo battito accettato. Un battito scartato non lo
    // aggiorna: vedi NodeHealthMonitor::heartbeat.
    MonoTime last_accepted{};

    std::uint64_t last_sequence{};
    std::uint64_t accepted{};
    std::uint64_t duplicates{};
    std::uint64_t regressions{};

    // Numero di messaggi mancanti rilevati dai salti di sequenza. Misura la
    // qualita' del collegamento, non il numero di episodi.
    std::uint64_t missed{};

    [[nodiscard]] bool ever_seen() const noexcept {
        return state != NodeState::unknown || accepted > 0;
    }
};

// Transizione di stato di un nodo, da registrare nella black box. Vale sia per i
// peggioramenti sia per i ripristini: sapere quando un nodo e' tornato e'
// necessario quanto sapere quando se n'e' andato.
struct NodeStateChange final {
    NodeId node{node_undeclared};
    NodeState previous{NodeState::unknown};
    NodeState current{NodeState::unknown};
    MonoTime timestamp{};
};

// Sorveglia il battito dei nodi dichiarati e ne deriva lo stato.
//
// Il monitor non conosce il trasporto: MAVLink, CAN o seriale chiamano
// heartbeat() con la sequenza dichiarata dal nodo. Questo tiene il core
// indipendente dai bus, come stabilito dalla ADR-002.
class NodeHealthMonitor final {
public:
    explicit NodeHealthMonitor(NodeHealthPolicy policy = {}) noexcept;

    // Dichiara un nodo atteso. Un nodo dichiarato e mai sentito resta `unknown`
    // e viene comunque riportato: un nodo che non si presenta e' esattamente
    // l'informazione che il pilota deve avere.
    void declare(NodeId node);

    // Registra un battito. Restituisce l'esito della valutazione della
    // sequenza.
    //
    // Un battito scartato, duplicato o arretrato, non aggiorna last_accepted:
    // un nodo bloccato che ripete all'infinito l'ultimo messaggio deve
    // invecchiare fino a stale e poi a lost, non apparire sano. E' la ragione
    // per cui il conteggio dei battiti non e' sufficiente e serve la sequenza.
    HeartbeatVerdict heartbeat(NodeId node, MonoTime at, std::uint64_t sequence);

    // Applica i timeout e restituisce le transizioni avvenute, in ordine di
    // dichiarazione dei nodi. Va chiamata a ogni ciclo, anche quando non arriva
    // nulla: e' il silenzio a dover produrre un evento.
    [[nodiscard]] std::vector<NodeStateChange> update(MonoTime now);

    [[nodiscard]] std::optional<NodeStatus> status(NodeId node) const;
    [[nodiscard]] const std::vector<NodeStatus>& nodes() const noexcept {
        return nodes_;
    }

    // Vero se tutti i nodi dichiarati sono online. Un solo nodo mai sentito e'
    // sufficiente a rendere falso il risultato.
    [[nodiscard]] bool all_online() const noexcept;

private:
    [[nodiscard]] NodeStatus* find(NodeId node) noexcept;

    NodeHealthPolicy policy_;
    std::vector<NodeStatus> nodes_;
    // heartbeat() puo' riportare online un nodo prima della successiva update().
    // La transizione resta qui finche' il ciclo di ingest non la raccoglie e la
    // scrive nella black box.
    std::vector<NodeStateChange> pending_changes_;
};

}  // namespace efis
