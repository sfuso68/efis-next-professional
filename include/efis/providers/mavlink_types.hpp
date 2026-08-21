#pragma once

#include <cstdint>

namespace efis::mavlink {

// Tipi dei campi MAVLink. `ch` e' un carattere, distinto da i8 perche' compare
// solo nei testi e non va interpretato come numero.
enum class FieldType : std::uint8_t { u8, i8, u16, i16, u32, i32, u64, i64, f32, f64, ch };

[[nodiscard]] constexpr std::uint8_t field_type_size(FieldType type) noexcept {
    switch (type) {
        case FieldType::u8:
        case FieldType::i8:
        case FieldType::ch: return 1;
        case FieldType::u16:
        case FieldType::i16: return 2;
        case FieldType::u32:
        case FieldType::i32:
        case FieldType::f32: return 4;
        case FieldType::u64:
        case FieldType::i64:
        case FieldType::f64: return 8;
    }
    return 0;
}

struct FieldDefinition final {
    const char* name;
    FieldType type;

    // Zero per un campo singolo, altrimenti il numero di elementi.
    std::uint8_t array_length;

    // Posizione nel payload, nell'ordine MAVLink per dimensione decrescente.
    std::uint8_t offset;

    [[nodiscard]] constexpr std::uint8_t element_size() const noexcept {
        return field_type_size(type);
    }

    [[nodiscard]] constexpr std::uint16_t total_size() const noexcept {
        const std::uint16_t elements = array_length == 0
            ? std::uint16_t{1}
            : static_cast<std::uint16_t>(array_length);
        return static_cast<std::uint16_t>(elements * element_size());
    }
};

struct MessageDefinition final {
    std::uint32_t id;
    const char* name;

    // Byte aggiuntivo previsto dal protocollo nel calcolo del checksum. Dipende
    // dalla definizione del messaggio e serve a far fallire il controllo quando
    // due estremi usano versioni incompatibili dello stesso identificativo.
    std::uint8_t crc_extra;

    // Lunghezza del payload completo, estensioni comprese.
    std::uint8_t length;

    // Lunghezza della parte definita alla nascita del messaggio. La distinzione
    // non e' formale: sotto questa soglia un frame corto ha soltanto gli zeri
    // finali rimossi, e i campi mancanti valgono davvero zero. Fra base_length e
    // length il trasmettitore quei campi non li conosce, e leggerli come zero
    // significherebbe inventare un dato, che il paragrafo 22 dell'handoff vieta.
    std::uint8_t base_length;

    const FieldDefinition* fields;
    std::uint8_t field_count;
};

}  // namespace efis::mavlink
