#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/node_health.hpp"
#include "efis/core/source_arbitrator.hpp"

#include <cstdint>
#include <iosfwd>
#include <vector>

namespace efis::recording {

// Versione dello schema del registro di volo. Un lettore che incontra una
// versione maggiore di quella che conosce si ferma invece di interpretare a
// meta': una registrazione letta male e' peggio di una non letta.
inline constexpr std::uint16_t flight_record_schema = 2;

// --- Perche' esiste una versione 2 -------------------------------------------
//
// Il formato V1 registrava tre segnali, velocita' indicata, quota e prua, e di
// ciascuno soltanto valore, sorgente e validita'. Tutto il resto dello snapshot
// non veniva scritto, e la provenienza introdotta nel campione non attraversava
// il confine della registrazione.
//
// Il risultato erano esattamente le registrazioni che il paragrafo 17.2
// dell'handoff vuole evitare: numeri di cui, a distanza di tempo, non si puo'
// piu' ricostruire il significato. Non si sapeva da quale nodo e da quale canale
// venissero, in quale unita' fossero espressi, quale taratura fosse in vigore,
// se fossero simulati, ne' quale fosse il valore grezzo prima della conversione.
//
// V1 aveva inoltre un difetto silenzioso: i numeri venivano scritti con la
// formattazione predefinita di ostream, sei cifre significative. Ogni valore
// registrato perdeva precisione rispetto al valore misurato, senza che nulla lo
// dichiarasse. Qui si scrive la rappresentazione piu' corta che rilegge
// esattamente lo stesso double.
//
// E soprattutto: V1 non aveva un lettore. Una scatola nera che non si sa
// rileggere non e' una scatola nera.

struct RecordedSample final {
    MonoTime timestamp{};

    // Il campione porta gia' la propria identita' in origin.signal: non viene
    // riscritta a parte per non poter divergere da se stessa.
    Sample<double> sample{};
};

struct RecordedNodeEvent final {
    MonoTime timestamp{};
    NodeId node{node_undeclared};
    NodeState previous{NodeState::unknown};
    NodeState current{NodeState::unknown};
};

// Esito della rilettura di un segmento.
struct FlightRecord final {
    std::uint16_t schema{};
    std::vector<RecordedSample> samples;
    std::vector<RecordedNodeEvent> node_events;
    std::vector<SourceChangeEvent> source_changes;

    // Righe che non si sono potute interpretare. Una registrazione interrotta
    // dalla perdita di alimentazione finisce a meta' riga, ed e' un caso atteso:
    // va contato e dichiarato, mai nascosto arrotondando il conto dei campioni.
    std::uint64_t malformed_rows{};

    // Righe di un tipo che questo lettore non conosce. Non sono un errore: sono
    // cio' che una versione successiva ha aggiunto e questa ignora.
    std::uint64_t unknown_rows{};

    // Vero se l'intestazione dichiarava una versione superiore a quella
    // supportata. In tal caso nulla e' stato letto.
    bool schema_too_new{};
};

class FlightRecordWriter final {
public:
    static void write_header(std::ostream& output);

    // Scrive ogni campione dello snapshot con la sua provenienza completa.
    static void write_snapshot(std::ostream& output, const FlightSnapshot& snapshot);

    static void write_sample(std::ostream& output, const Sample<double>& sample,
                             MonoTime timestamp);
    static void write_node_event(std::ostream& output, const NodeStateChange& event);
    static void write_source_change(std::ostream& output, const SourceChangeEvent& event);
};

// Rilegge un segmento. Il replay del paragrafo 17.3 dell'handoff comincia qui:
// senza rilettura non esiste ne' verifica ne' riproduzione deterministica.
[[nodiscard]] FlightRecord read_flight_record(std::istream& input);

}  // namespace efis::recording
