#include "efis/providers/dronecan.hpp"

#include "efis/providers/dronecan_type_table.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace efis::dronecan {
namespace {

// Legge bit consecutivi, perche' il DSDL non allinea i campi ai byte.
//
// I bit di ogni byte si leggono dal piu' significativo, ed e' l'unica
// convenzione che rende il payload interpretabile: leggendoli dal meno
// significativo tornano numeri plausibili e sbagliati, che e' il modo peggiore
// di avere torto.
class BitReader final {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data), bits_(size * 8) {}

    [[nodiscard]] std::size_t remaining() const noexcept { return bits_ - position_; }

    [[nodiscard]] bool read(std::uint8_t width, std::uint64_t& out) noexcept {
        if (width == 0 || width > 64 || remaining() < width) return false;

        // Un valore si dispone sul filo in byte little-endian, e ogni byte si
        // scrive dal bit piu' significativo. Le due convenzioni convivono, ed e'
        // la parte che si sbaglia leggendo la specifica invece che i byte: un
        // campo da diciassette bit occupa due byte a partire dal meno
        // significativo, piu' un resto di un bit che porta i bit alti.
        //
        // Determinato accendendo un bit per volta con l'implementazione di
        // riferimento e guardando dove finiva, non dedotto.
        std::uint64_t value = 0;
        const std::uint8_t whole = static_cast<std::uint8_t>(width / 8);
        const std::uint8_t rest = static_cast<std::uint8_t>(width % 8);
        for (std::uint8_t index = 0; index < whole; ++index) {
            value |= static_cast<std::uint64_t>(take(8)) << (8U * index);
        }
        if (rest != 0) {
            value |= static_cast<std::uint64_t>(take(rest)) << (8U * whole);
        }
        out = value;
        return true;
    }

private:
    // Preleva bit consecutivi dal flusso, dal piu' significativo di ogni byte.
    [[nodiscard]] std::uint64_t take(std::uint8_t width) noexcept {
        std::uint64_t value = 0;
        for (std::uint8_t index = 0; index < width; ++index) {
            const std::size_t bit = position_ + index;
            const std::uint8_t byte = data_[bit / 8];
            const std::uint8_t shift = static_cast<std::uint8_t>(7 - (bit % 8));
            value = (value << 1) | static_cast<std::uint64_t>((byte >> shift) & 1U);
        }
        position_ += width;
        return value;
    }

public:
    [[nodiscard]] bool skip(std::uint8_t width) noexcept {
        std::uint64_t discarded = 0;
        return read(width, discarded);
    }

private:
    const std::uint8_t* data_;
    std::size_t bits_;
    std::size_t position_{};
};

// IEEE 754 a mezza precisione. Non esiste come tipo nativo, e il DSDL lo usa per
// quasi tutte le grandezze fisiche: temperature, pressioni, portate.
double from_float16(std::uint16_t bits) noexcept {
    const std::uint16_t sign = static_cast<std::uint16_t>((bits >> 15) & 0x1);
    const std::uint16_t exponent = static_cast<std::uint16_t>((bits >> 10) & 0x1F);
    const std::uint16_t mantissa = static_cast<std::uint16_t>(bits & 0x3FF);

    double value = 0.0;
    if (exponent == 0) {
        // Subnormale, oppure zero.
        value = std::ldexp(static_cast<double>(mantissa), -24);
    } else if (exponent == 0x1F) {
        value = mantissa == 0 ? std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::quiet_NaN();
    } else {
        value = std::ldexp(static_cast<double>(mantissa) + 1024.0,
                           static_cast<int>(exponent) - 25);
    }
    return sign != 0 ? -value : value;
}

double from_float32(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return static_cast<double>(value);
}

double from_float64(std::uint64_t bits) noexcept {
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double as_signed(std::uint64_t bits, std::uint8_t width) noexcept {
    // Estensione del segno da una larghezza qualunque: il DSDL usa anche
    // interi che non corrispondono a un tipo nativo.
    const std::uint64_t sign_bit = std::uint64_t{1} << (width - 1);
    if ((bits & sign_bit) == 0) return static_cast<double>(bits);
    const std::uint64_t magnitude = (~bits + 1U) & ((std::uint64_t{1} << width) - 1U);
    return -static_cast<double>(magnitude);
}

std::size_t total_bits(const TypeDescriptor& type) noexcept {
    std::size_t bits = 0;
    for (std::uint8_t index = 0; index < type.field_count; ++index) {
        bits += type.fields[index].bits;
    }
    return bits;
}

bool decode_fields(const TypeDescriptor& type, BitReader& reader,
                   std::vector<DecodedField>& out, std::size_t element_index);

bool decode_one(const FieldDescriptor& field, BitReader& reader,
                std::vector<DecodedField>& out, std::size_t element_index) {
    switch (field.shape) {
        case FieldShape::padding:
            return reader.skip(field.bits);

        case FieldShape::boolean:
        case FieldShape::unsigned_integer: {
            std::uint64_t bits = 0;
            if (!reader.read(field.bits, bits)) return false;
            out.push_back({field.name, element_index, static_cast<double>(bits), bits});
            return true;
        }

        case FieldShape::signed_integer: {
            std::uint64_t bits = 0;
            if (!reader.read(field.bits, bits)) return false;
            out.push_back({field.name, element_index, as_signed(bits, field.bits), bits});
            return true;
        }

        case FieldShape::floating: {
            std::uint64_t bits = 0;
            if (!reader.read(field.bits, bits)) return false;
            double value = 0.0;
            if (field.bits == 16) {
                value = from_float16(static_cast<std::uint16_t>(bits));
            } else if (field.bits == 32) {
                value = from_float32(static_cast<std::uint32_t>(bits));
            } else if (field.bits == 64) {
                value = from_float64(bits);
            } else {
                return false;
            }
            out.push_back({field.name, element_index, value, bits});
            return true;
        }

        case FieldShape::compound:
        case FieldShape::array:
            // Gestiti da decode_fields, che conosce la posizione del campo
            // nell'elenco e quindi sa se e' l'ultimo.
            return false;
    }
    return false;
}

bool decode_array(const FieldDescriptor& field, BitReader& reader,
                  std::vector<DecodedField>& out, bool is_last) {
    // L'elemento e' una struttura: nella tabella e' il tipo che porta lo stesso
    // nome del campo. Gli array di primitivi non compaiono nei tipi che
    // interpretiamo, e non vengono indovinati.
    const TypeDescriptor* element = find_type_by_name("uavcan.equipment.ice.reciprocating.CylinderStatus");
    if (field.name == std::string_view{"cylinder_status"} && element != nullptr) {
        const std::size_t element_bits = total_bits(*element);
        if (element_bits == 0) return false;

        std::size_t count = 0;
        if (field.dynamic && !(is_last && element_bits >= 8)) {
            // Array dinamico non in coda: sul filo lo precede il numero di
            // elementi, su tanti bit quanti ne servono a contare fino al massimo.
            std::uint8_t width = 1;
            while ((std::size_t{1} << width) < static_cast<std::size_t>(field.max_size) + 1U) {
                ++width;
            }
            std::uint64_t declared = 0;
            if (!reader.read(width, declared)) return false;
            count = static_cast<std::size_t>(declared);
        } else {
            // Ottimizzazione di coda del DSDL: un array dinamico che chiude il
            // messaggio, con elementi da almeno otto bit, non porta il numero di
            // elementi. Si ricava da quanti bit restano, ed e' l'unico modo:
            // cercare un prefisso che non c'e' sposterebbe tutto.
            count = reader.remaining() / element_bits;
        }

        if (count > field.max_size) return false;
        for (std::size_t index = 0; index < count; ++index) {
            if (!decode_fields(*element, reader, out, index)) return false;
        }
        return true;
    }
    return false;
}

bool decode_fields(const TypeDescriptor& type, BitReader& reader,
                   std::vector<DecodedField>& out, std::size_t element_index) {
    for (std::uint8_t index = 0; index < type.field_count; ++index) {
        const FieldDescriptor& field = type.fields[index];
        const bool is_last = index + 1 == type.field_count;
        if (field.shape == FieldShape::array) {
            if (!decode_array(field, reader, out, is_last)) return false;
        } else if (field.shape == FieldShape::compound) {
            return false;
        } else if (!decode_one(field, reader, out, element_index)) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::uint16_t crc16(const std::uint8_t* data, std::size_t size,
                    std::uint16_t seed) noexcept {
    std::uint16_t crc = seed;
    for (std::size_t index = 0; index < size; ++index) {
        crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(data[index]) << 8));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) != 0
                      ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                      : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

const TypeDescriptor* find_type(std::uint32_t data_type_id) noexcept {
    for (const TypeDescriptor& type : generated::types) {
        if (type.data_type_id == data_type_id && data_type_id != 0) return &type;
    }
    return nullptr;
}

const TypeDescriptor* find_type_by_name(std::string_view name) noexcept {
    for (const TypeDescriptor& type : generated::types) {
        if (name == type.name) return &type;
    }
    return nullptr;
}

std::optional<double> DecodedMessage::value(std::string_view name,
                                            std::size_t index) const noexcept {
    for (const DecodedField& field : fields) {
        if (field.name == name && field.index == index) return field.value;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> DecodedMessage::raw(std::string_view name,
                                                 std::size_t index) const noexcept {
    for (const DecodedField& field : fields) {
        if (field.name == name && field.index == index) return field.bits;
    }
    return std::nullopt;
}

std::size_t DecodedMessage::count(std::string_view name) const noexcept {
    std::size_t highest = 0;
    bool seen = false;
    for (const DecodedField& field : fields) {
        if (field.name != name) continue;
        seen = true;
        highest = std::max(highest, field.index);
    }
    return seen ? highest + 1 : 0;
}

std::optional<DecodedMessage> decode(const TypeDescriptor& type,
                                     const std::uint8_t* payload, std::size_t size) {
    BitReader reader{payload, size};
    DecodedMessage message{};
    message.type = &type;
    if (!decode_fields(type, reader, message.fields, 0)) return std::nullopt;
    return message;
}

std::optional<DecodedMessage> Reassembler::push(const can::Frame& frame) {
    if (frame.length == 0) return std::nullopt;

    const MessageHeader header = decode_message_id(frame.id);
    if (header.service) {
        ++statistics_.service_frames;
        return std::nullopt;
    }

    const TypeDescriptor* type = find_type(header.data_type_id);
    if (type == nullptr) {
        // Senza la signature il CRC non e' calcolabile, quindi il trasferimento
        // non e' verificabile. Si conta e si lascia perdere.
        ++statistics_.unknown_types;
        return std::nullopt;
    }

    const Tail tail = decode_tail(frame.data[frame.length - 1]);
    const std::uint8_t body_length = static_cast<std::uint8_t>(frame.length - 1);

    // Frame singolo: nessun CRC di trasferimento, il messaggio e' tutto qui.
    if (tail.start && tail.end) {
        ++statistics_.transfers;
        ++statistics_.single_frame;
        auto decoded = decode(*type, frame.data.data(), body_length);
        if (decoded.has_value()) {
            decoded->source_node = header.source_node;
            decoded->transfer_id = tail.transfer_id;
        }
        return decoded;
    }

    auto it = std::find_if(partials_.begin(), partials_.end(),
                           [&](const Partial& partial) {
                               return partial.source_node == header.source_node &&
                                      partial.data_type_id == header.data_type_id;
                           });

    if (tail.start) {
        if (body_length < 2) return std::nullopt;
        if (it != partials_.end()) {
            // Un nuovo trasferimento dallo stesso nodo prima che il precedente
            // finisse: quello vecchio non si completera' piu'.
            ++statistics_.abandoned;
            partials_.erase(it);
        }
        Partial partial{};
        partial.source_node = header.source_node;
        partial.data_type_id = header.data_type_id;
        partial.transfer_id = tail.transfer_id;
        partial.declared_crc = static_cast<std::uint16_t>(
            frame.data[0] | (static_cast<std::uint16_t>(frame.data[1]) << 8));
        partial.expect_toggle = true;
        partial.payload.assign(frame.data.begin() + 2,
                               frame.data.begin() + body_length);
        partials_.push_back(std::move(partial));
        return std::nullopt;
    }

    if (it == partials_.end()) {
        // Frame intermedio senza inizio: la cattura comincia a meta' di un
        // trasferimento, oppure il primo frame si e' perso.
        return std::nullopt;
    }

    if (tail.transfer_id != it->transfer_id || tail.toggle != it->expect_toggle) {
        ++statistics_.toggle_errors;
        partials_.erase(it);
        return std::nullopt;
    }
    it->expect_toggle = !it->expect_toggle;

    if (it->payload.size() + body_length > max_transfer_bytes) {
        ++statistics_.abandoned;
        partials_.erase(it);
        return std::nullopt;
    }
    it->payload.insert(it->payload.end(), frame.data.begin(),
                       frame.data.begin() + body_length);

    if (!tail.end) return std::nullopt;

    // Il CRC copre la signature del tipo, in little-endian, seguita dal payload.
    std::array<std::uint8_t, 8> signature{};
    for (std::size_t index = 0; index < signature.size(); ++index) {
        signature[index] = static_cast<std::uint8_t>((type->signature >> (8U * index)) & 0xFFU);
    }
    std::uint16_t computed = crc16(signature.data(), signature.size());
    computed = crc16(it->payload.data(), it->payload.size(), computed);

    const Partial finished = std::move(*it);
    partials_.erase(it);

    if (computed != finished.declared_crc) {
        ++statistics_.crc_errors;
        return std::nullopt;
    }

    ++statistics_.transfers;
    auto decoded = decode(*type, finished.payload.data(), finished.payload.size());
    if (decoded.has_value()) {
        decoded->source_node = finished.source_node;
        decoded->transfer_id = finished.transfer_id;
    }
    return decoded;
}

}  // namespace efis::dronecan
