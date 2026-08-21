#pragma once

#include "efis/providers/can_frame.hpp"
#include "efis/providers/dronecan_types.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace efis::dronecan {

inline constexpr std::size_t max_transfer_bytes = 512;

// Byte di coda, presente in fondo a ogni frame.
struct Tail final {
    bool start{};
    bool end{};
    bool toggle{};
    std::uint8_t transfer_id{};
};

[[nodiscard]] constexpr Tail decode_tail(std::uint8_t byte) noexcept {
    return {(byte & 0x80) != 0, (byte & 0x40) != 0, (byte & 0x20) != 0,
            static_cast<std::uint8_t>(byte & 0x1F)};
}

// Scomposizione dell'identificatore a 29 bit di un messaggio broadcast.
struct MessageHeader final {
    std::uint8_t priority{};
    std::uint16_t data_type_id{};
    bool service{};
    std::uint8_t source_node{};
};

[[nodiscard]] constexpr MessageHeader decode_message_id(std::uint32_t id) noexcept {
    return {static_cast<std::uint8_t>((id >> 24) & 0x1F),
            static_cast<std::uint16_t>((id >> 8) & 0xFFFF),
            ((id >> 7) & 1) != 0,
            static_cast<std::uint8_t>(id & 0x7F)};
}

// CRC-16-CCITT, quello che DroneCAN usa per il CRC del trasferimento.
[[nodiscard]] std::uint16_t crc16(const std::uint8_t* data, std::size_t size,
                                  std::uint16_t seed = 0xFFFF) noexcept;

[[nodiscard]] const TypeDescriptor* find_type(std::uint32_t data_type_id) noexcept;
[[nodiscard]] const TypeDescriptor* find_type_by_name(std::string_view name) noexcept;

// Un campo decodificato.
//
// `index` distingue gli elementi di un array: i quattro cilindri producono
// quattro volte lo stesso nome con indice diverso, invece di nomi inventati.
struct DecodedField final {
    std::string_view name;
    std::size_t index{};
    double value{};

    // Rappresentazione grezza, prima dell'interpretazione. Serve alle maschere
    // di bit, che un double non conserva esatte.
    std::uint64_t bits{};
};

struct DecodedMessage final {
    const TypeDescriptor* type{};
    std::uint8_t source_node{};
    std::uint8_t transfer_id{};
    std::vector<DecodedField> fields;

    [[nodiscard]] std::optional<double> value(std::string_view name,
                                              std::size_t index = 0) const noexcept;
    [[nodiscard]] std::optional<std::uint64_t> raw(std::string_view name,
                                                   std::size_t index = 0) const noexcept;

    // Quanti elementi ha prodotto un array.
    [[nodiscard]] std::size_t count(std::string_view name) const noexcept;
};

struct ReassemblyStatistics final {
    std::uint64_t transfers{};
    std::uint64_t single_frame{};

    // CRC del trasferimento non corrispondente. Come per MAVLink, puo' essere
    // disturbo, ma anche una signature sbagliata in tabella: in quel caso
    // fallisce sempre e soltanto un tipo, ed e' cosi' che l'errore si manifesta.
    std::uint64_t crc_errors{};

    // Bit di toggle fuori sequenza: un frame perso o duplicato. Il trasferimento
    // viene abbandonato invece che ricucito, perche' ricucirlo significherebbe
    // consegnare un messaggio con un pezzo mancante.
    std::uint64_t toggle_errors{};

    // Trasferimenti abbandonati perche' ne e' cominciato un altro dallo stesso
    // nodo prima che finissero.
    std::uint64_t abandoned{};

    // Frame di un tipo che non e' in tabella. Senza la signature il CRC non e'
    // calcolabile, quindi il trasferimento non e' verificabile.
    std::uint64_t unknown_types{};

    // Frame di servizio, cioe' richieste e risposte punto a punto. EFIS NEXT non
    // ne emette e non ne interpreta: e' un ascoltatore.
    std::uint64_t service_frames{};
};

// Riassembla i frame in trasferimenti e li decodifica.
//
// Di sola lettura: consuma frame e non ne produce. Non risponde, non chiede un
// identificativo di nodo, non emette nulla sul bus.
class Reassembler final {
public:
    // Restituisce un messaggio quando un trasferimento e' completo e verificato.
    [[nodiscard]] std::optional<DecodedMessage> push(const can::Frame& frame);

    [[nodiscard]] const ReassemblyStatistics& statistics() const noexcept {
        return statistics_;
    }

    void reset() noexcept { partials_.clear(); }

private:
    // Un trasferimento in corso, per nodo e tipo.
    struct Partial final {
        std::uint8_t source_node{};
        std::uint16_t data_type_id{};
        std::uint8_t transfer_id{};
        bool expect_toggle{};
        std::uint16_t declared_crc{};
        std::vector<std::uint8_t> payload;
    };

    ReassemblyStatistics statistics_{};
    std::vector<Partial> partials_;
};

// Decodifica un payload gia' riassemblato secondo il tipo indicato.
[[nodiscard]] std::optional<DecodedMessage> decode(const TypeDescriptor& type,
                                                   const std::uint8_t* payload,
                                                   std::size_t size);

}  // namespace efis::dronecan
