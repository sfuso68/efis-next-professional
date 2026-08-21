#pragma once

#include "efis/providers/mavlink_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace efis::mavlink {

inline constexpr std::uint8_t stx_v1 = 0xFE;
inline constexpr std::uint8_t stx_v2 = 0xFD;
inline constexpr std::size_t max_payload = 255;

// Bit 0 dei flag di incompatibilita': il frame porta in coda tredici byte di
// firma. Non consumarli significherebbe leggerli come inizio del messaggio
// successivo e perdere la sincronia.
inline constexpr std::uint8_t incompat_signed = 0x01;
inline constexpr std::size_t signature_length = 13;

struct Frame final {
    std::uint32_t message_id{};
    std::uint8_t system_id{};
    std::uint8_t component_id{};
    std::uint8_t sequence{};
    bool version2{};
    bool signed_frame{};

    // Lunghezza effettivamente ricevuta, che puo' essere inferiore a quella del
    // messaggio: MAVLink 2 elimina gli zeri finali del payload.
    std::uint8_t payload_length{};
    std::array<std::uint8_t, max_payload> payload{};
};

// Cio' che il parser incontra e che non produce un frame. Sono numeri da
// mostrare, non da nascondere: un collegamento che degrada si vede qui prima
// che nei valori.
struct ParserStatistics final {
    std::uint64_t frames{};
    std::uint64_t frames_v1{};
    std::uint64_t frames_signed{};

    // Checksum non corrispondente. Puo' essere disturbo sulla linea, ma anche un
    // CRC_EXTRA sbagliato in tabella: in quel caso fallisce sempre e soltanto un
    // tipo di messaggio, ed e' cosi' che l'errore si manifesta.
    std::uint64_t crc_errors{};

    // Frame il cui identificativo non e' in tabella. Senza il suo CRC_EXTRA il
    // checksum non e' calcolabile, quindi il frame non e' verificabile: viene
    // scartato, non consegnato con riserva.
    std::uint64_t unknown_messages{};

    // Byte scartati mentre si cerca un inizio di frame.
    std::uint64_t dropped_bytes{};
};

// Parser MAVLink 1 e 2, un byte per volta.
//
// Il chiamante non deve allineare i dati ai confini dei frame, cosa impossibile
// leggendo da una seriale: versa i byte nell'ordine in cui arrivano e riceve un
// frame quando ne e' stato completato e verificato uno.
//
// Non esiste alcun percorso di trasmissione, e non e' una dimenticanza: questo
// e' un ascoltatore. Il paragrafo 25 dell'handoff colloca l'osservazione in sola
// lettura al primo livello, separato dalla configurazione e dal comando.
class Parser final {
public:
    // Versa byte nell'ordine in cui arrivano, senza allinearli ai confini dei
    // frame: leggendo da una seriale allinearli e' impossibile.
    void push(std::span<const std::uint8_t> bytes);

    // Estrae il prossimo frame verificato, consumando i byte necessari.
    // Restituisce nullopt quando servono altri dati.
    [[nodiscard]] std::optional<Frame> next();

    [[nodiscard]] const ParserStatistics& statistics() const noexcept {
        return statistics_;
    }

    void reset() noexcept;

private:
    std::vector<std::uint8_t> buffer_;
    std::size_t begin_{};
    ParserStatistics statistics_{};
};

// --- Tabella dei messaggi ----------------------------------------------------

[[nodiscard]] const MessageDefinition* find_message(std::uint32_t id) noexcept;

[[nodiscard]] const FieldDefinition* find_field(const MessageDefinition& message,
                                                std::string_view name) noexcept;

// --- Lettura dei campi -------------------------------------------------------

// La distinzione fra "assente" e "zero" e' la ragione per cui la lettura non
// restituisce semplicemente un numero. Un campo di estensione che il
// trasmettitore non conosce non vale zero: non c'e'. Restituire zero
// significherebbe presentare un dato inventato come una misura, che il
// paragrafo 22 dell'handoff vieta.
enum class FieldStatus : std::uint8_t {
    present,

    // Campo previsto dal messaggio ma oltre la parte ricevuta e oltre la
    // lunghezza base: il trasmettitore appartiene a una versione che non lo
    // definisce.
    not_transmitted,

    // Il messaggio non ha un campo con questo nome, oppure l'indice richiesto e'
    // fuori dall'array.
    unknown_field
};

struct FieldRead final {
    double value{};

    // Rappresentazione grezza, prima di qualunque interpretazione di segno o
    // virgola mobile. Serve alle maschere di bit e ai tempi a 64 bit, che un
    // double non conserva esatti.
    std::uint64_t bits{};
    FieldType type{FieldType::u8};
    FieldStatus status{FieldStatus::unknown_field};

    [[nodiscard]] constexpr bool ok() const noexcept {
        return status == FieldStatus::present;
    }
};

[[nodiscard]] FieldRead read_field(const MessageDefinition& message,
                                   const Frame& frame,
                                   std::string_view name,
                                   std::size_t index = 0) noexcept;

// Valore convertito in virgola mobile. Adatto alle grandezze fisiche, non alle
// maschere di bit ne' ai tempi a 64 bit, che un double non conserva esatti.
[[nodiscard]] std::optional<double> read_number(const MessageDefinition& message,
                                                const Frame& frame,
                                                std::string_view name,
                                                std::size_t index = 0) noexcept;

[[nodiscard]] std::optional<std::int64_t> read_integer(const MessageDefinition& message,
                                                       const Frame& frame,
                                                       std::string_view name,
                                                       std::size_t index = 0) noexcept;

// Necessaria per le maschere di stato dei sensori e per i tempi in microsecondi:
// passare per un double li manterrebbe esatti solo per caso, e perdere un bit di
// una maschera significa perdere lo stato di un sensore.
[[nodiscard]] std::optional<std::uint64_t> read_unsigned(const MessageDefinition& message,
                                                         const Frame& frame,
                                                         std::string_view name,
                                                         std::size_t index = 0) noexcept;

// Campo di caratteri, non necessariamente terminato. ArduPilot lo usa per gli
// avvisi in STATUSTEXT.
[[nodiscard]] std::string read_text(const MessageDefinition& message,
                                    const Frame& frame,
                                    std::string_view name);

}  // namespace efis::mavlink
