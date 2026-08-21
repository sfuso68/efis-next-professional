#pragma once

#include "efis/core/types.hpp"
#include "efis/io/byte_source.hpp"
#include "efis/providers/mavlink_adapter.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace efis::bench {

// Banco prova dei sensori: osserva cio' che il Pixhawk trasmette e lo presenta
// senza mai promuoverlo a sorgente di volo.
//
// E' il passo 6 del backbone dati P1: collegare GNSS, assetto, barometro e
// sensore aria al banco della UI in sola osservazione. La parola che conta e'
// osservazione. Nulla di quanto esce da qui alimenta il modello di volo, la
// guida o gli allarmi: il banco esiste per far vedere a terra che i dati veri
// arrivano e che significato hanno, prima che qualcuno decida di usarli.
//
// La distinzione non e' formale. Il paragrafo 25 dell'handoff separa
// l'osservazione dalla configurazione, dalla calibrazione e dall'attivazione nel
// controllo di volo, e chiede che ogni passaggio fra questi livelli sia
// deliberato. Un banco che si limiti a mostrare i numeri accanto a quelli del
// PFD renderebbe quel passaggio invisibile, ed e' esattamente cio' che non deve
// accadere.

// Motivo per cui una riga non e' utilizzabile. Serve a distinguere una misura
// che manca da una che c'e' e non ci si puo' fidare: sono guasti diversi e
// richiedono azioni diverse.
enum class BenchNote : std::uint8_t {
    none,
    never_received,
    no_gnss_fix,
    sensor_declared_unhealthy,
    stale,
    simulated
};

[[nodiscard]] constexpr const char* bench_note_text(BenchNote note) noexcept {
    switch (note) {
        case BenchNote::none: return "";
        case BenchNote::never_received: return "mai ricevuto";
        case BenchNote::no_gnss_fix: return "nessun fix tridimensionale";
        case BenchNote::sensor_declared_unhealthy: return "sensore dichiarato non sano";
        case BenchNote::stale: return "dato scaduto";
        case BenchNote::simulated: return "dato simulato";
    }
    return "";
}

// Una grandezza osservata, con tutto cio' che serve a mostrarla onestamente.
struct BenchRow final {
    SignalId signal{SignalId::unknown};
    double value{};

    // Valore prima della conversione. Su questo banco non e' un dettaglio: e'
    // cio' che permette di distinguere un sensore fermo da una conversione
    // sbagliata, e per il sensore aria e' la misura vera, mentre la velocita'
    // e' gia' un'interpretazione.
    double raw{};

    Unit unit{Unit::unknown};
    Validity validity{Validity::unavailable};
    SourceId source{SourceId::unknown};
    NodeId node{node_undeclared};

    // Tempo trascorso dall'ultimo aggiornamento. Un banco che non mostrasse
    // l'eta' lascerebbe un valore fermo indistinguibile da uno appena arrivato.
    std::chrono::milliseconds age{};

    bool simulated{};

    // Esito unico su cui la UI deve basarsi. Non coincide con la sola validita':
    // tiene conto anche del fix satellitare e della salute dichiarata del
    // sensore, che arrivano in messaggi diversi da quelli che portano il valore.
    bool usable{};
    BenchNote note{BenchNote::never_received};

    [[nodiscard]] const char* name() const noexcept { return signal_name(signal); }
    [[nodiscard]] const char* symbol() const noexcept { return unit_symbol(unit); }
    [[nodiscard]] const char* note_text() const noexcept { return bench_note_text(note); }
};

struct BenchView final {
    std::vector<BenchRow> rows;
    std::vector<NodeStatus> nodes;

    std::int64_t gnss_fix_type{};
    std::int64_t gnss_satellites{};

    bool air_data_present{};
    bool air_data_healthy{};

    // Sempre falso, e dichiarato esplicitamente invece che sottinteso. Il
    // paragrafo 14 tiene ARSPD_USE a zero fino al termine della validazione in
    // volo: finche' e' cosi', questo banco non deve poter essere scambiato per
    // una sorgente abilitata. Il giorno in cui cambiera' dovra' cambiare qui,
    // deliberatamente.
    bool feeds_flight_control{false};

    std::uint64_t frames{};
    std::uint64_t crc_errors{};
    std::uint64_t unknown_messages{};
    std::uint64_t dropped_bytes{};

    // Ultimo avviso testuale dell'autopilota, tipicamente un motivo di pre-arm.
    std::string status_text;

    [[nodiscard]] const BenchRow* find(SignalId signal) const noexcept;
    [[nodiscard]] bool receiving() const noexcept { return frames > 0; }
};

// Legge byte da una sorgente qualsiasi e ne ricava la vista del banco.
//
// La sorgente e' astratta di proposito: la stessa catena alimenta il banco da
// una registrazione o da una seriale senza sapere la differenza, che e' cio' che
// rende riproducibile una prova. Vedi ADR-003.
class SensorBench final {
public:
    explicit SensorBench(std::unique_ptr<io::ByteSource> source,
                         NodeHealthPolicy policy = {});

    // Legge cio' che e' disponibile e aggiorna la vista. Va chiamata anche
    // quando non arriva nulla: e' il silenzio a dover far invecchiare i valori.
    void pump(MonoTime now);

    [[nodiscard]] const BenchView& view() const noexcept { return view_; }
    [[nodiscard]] const MavlinkAdapter& adapter() const noexcept { return adapter_; }
    [[nodiscard]] bool exhausted() const noexcept;

private:
    void rebuild(MonoTime now);

    std::unique_ptr<io::ByteSource> source_;
    MavlinkAdapter adapter_;
    BenchView view_{};
    std::vector<std::uint8_t> buffer_;
};

}  // namespace efis::bench
