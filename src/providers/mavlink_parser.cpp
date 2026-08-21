#include "efis/providers/mavlink_parser.hpp"

#include "efis/providers/mavlink_message_table.hpp"

#include <algorithm>
#include <cstring>

namespace efis::mavlink {
namespace {

// CRC-16/MCRF4XX, che il protocollo chiama checksum X.25. Polinomio riflesso
// 0x8408, valore iniziale 0xFFFF.
constexpr void accumulate(std::uint8_t byte, std::uint16_t& crc) noexcept {
    auto tmp = static_cast<std::uint8_t>(byte ^ static_cast<std::uint8_t>(crc & 0xFF));
    tmp = static_cast<std::uint8_t>(tmp ^ static_cast<std::uint8_t>(tmp << 4));
    crc = static_cast<std::uint16_t>(
        (crc >> 8) ^ (static_cast<std::uint16_t>(tmp) << 8) ^
        (static_cast<std::uint16_t>(tmp) << 3) ^ (static_cast<std::uint16_t>(tmp) >> 4));
}

std::uint64_t load(const std::uint8_t* bytes, std::uint8_t size) noexcept {
    std::uint64_t value = 0;
    for (std::uint8_t i = 0; i < size; ++i) {
        value |= static_cast<std::uint64_t>(bytes[i]) << (8U * i);
    }
    return value;
}

std::int64_t as_signed(std::uint64_t bits, FieldType type) noexcept {
    switch (type) {
        case FieldType::i8:
        case FieldType::ch: return static_cast<std::int8_t>(bits);
        case FieldType::i16: return static_cast<std::int16_t>(bits);
        case FieldType::i32: return static_cast<std::int32_t>(bits);
        case FieldType::i64: return static_cast<std::int64_t>(bits);
        default: return static_cast<std::int64_t>(bits);
    }
}

double as_number(std::uint64_t bits, FieldType type) noexcept {
    switch (type) {
        case FieldType::f32: {
            const auto narrowed = static_cast<std::uint32_t>(bits);
            float value{};
            std::memcpy(&value, &narrowed, sizeof(value));
            return static_cast<double>(value);
        }
        case FieldType::f64: {
            double value{};
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        case FieldType::u8:
        case FieldType::u16:
        case FieldType::u32:
        case FieldType::u64:
            return static_cast<double>(bits);
        default:
            return static_cast<double>(as_signed(bits, type));
    }
}

}  // namespace

void Parser::push(std::span<const std::uint8_t> bytes) {
    // Si compatta ogni tanto e non a ogni frame: spostare la coda dopo ciascun
    // messaggio costerebbe piu' della memoria che libera.
    if (begin_ > 4096) {
        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + static_cast<std::ptrdiff_t>(begin_));
        begin_ = 0;
    }
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

void Parser::reset() noexcept {
    buffer_.clear();
    begin_ = 0;
}

std::optional<Frame> Parser::next() {
    while (true) {
        const std::size_t available = buffer_.size() - begin_;
        if (available < 8) return std::nullopt;

        const std::uint8_t* head = buffer_.data() + begin_;
        const std::uint8_t start = head[0];
        if (start != stx_v1 && start != stx_v2) {
            ++begin_;
            ++statistics_.dropped_bytes;
            continue;
        }

        const bool version2 = start == stx_v2;
        const std::uint8_t payload_length = head[1];
        const std::size_t header = version2 ? 10U : 6U;
        const bool signed_frame = version2 && (head[2] & incompat_signed) != 0;
        const std::size_t signature = signed_frame ? signature_length : 0U;
        const std::size_t total = header + payload_length + 2U + signature;

        if (available < total) return std::nullopt;

        std::uint32_t message_id{};
        if (version2) {
            message_id = static_cast<std::uint32_t>(head[7]) |
                         (static_cast<std::uint32_t>(head[8]) << 8) |
                         (static_cast<std::uint32_t>(head[9]) << 16);
        } else {
            message_id = head[5];
        }

        const MessageDefinition* definition = find_message(message_id);
        if (definition == nullptr) {
            // Senza il CRC_EXTRA il checksum non e' calcolabile, quindi non si
            // puo' sapere ne' se il frame sia integro ne' se la lunghezza
            // dichiarata sia attendibile. Si riparte dal byte successivo invece
            // di fidarsi di un campo potenzialmente corrotto.
            ++statistics_.unknown_messages;
            ++begin_;
            continue;
        }

        std::uint16_t crc = 0xFFFF;
        for (std::size_t i = 1; i < header + payload_length; ++i) accumulate(head[i], crc);
        accumulate(definition->crc_extra, crc);

        const auto received = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(head[header + payload_length]) |
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(head[header + payload_length + 1]) << 8));

        if (received != crc) {
            ++statistics_.crc_errors;
            ++begin_;
            continue;
        }

        Frame frame{};
        frame.message_id = message_id;
        frame.version2 = version2;
        // La firma non viene verificata: richiede le chiavi condivise, che
        // questo nodo non possiede e che il paragrafo 19 dell'handoff vieta di
        // tenere nel repository. Va pero' dichiarata, perche' un frame firmato
        // e non verificato non e' un frame autenticato.
        frame.signed_frame = signed_frame;
        if (version2) {
            frame.sequence = head[4];
            frame.system_id = head[5];
            frame.component_id = head[6];
        } else {
            frame.sequence = head[2];
            frame.system_id = head[3];
            frame.component_id = head[4];
        }
        frame.payload_length = payload_length;
        std::copy_n(head + header, payload_length, frame.payload.begin());

        // I byte di firma vengono consumati con il resto del frame: lasciarli
        // nel buffer li farebbe leggere come inizio del messaggio successivo.
        begin_ += total;

        ++statistics_.frames;
        if (!version2) ++statistics_.frames_v1;
        if (signed_frame) ++statistics_.frames_signed;
        return frame;
    }
}

const MessageDefinition* find_message(std::uint32_t id) noexcept {
    for (const MessageDefinition& message : generated::messages) {
        if (message.id == id) return &message;
    }
    return nullptr;
}

const FieldDefinition* find_field(const MessageDefinition& message,
                                  std::string_view name) noexcept {
    for (std::uint8_t i = 0; i < message.field_count; ++i) {
        if (name == message.fields[i].name) return &message.fields[i];
    }
    return nullptr;
}

FieldRead read_field(const MessageDefinition& message, const Frame& frame,
                     std::string_view name, std::size_t index) noexcept {
    const FieldDefinition* field = find_field(message, name);
    if (field == nullptr) return {};

    const std::size_t elements = field->array_length == 0 ? 1U : field->array_length;
    if (index >= elements) return {};

    const std::uint8_t element = field->element_size();
    const std::size_t offset = field->offset + index * element;
    const std::size_t end = offset + element;

    if (end <= frame.payload_length) {
        const std::uint64_t bits = load(frame.payload.data() + offset, element);
        return {as_number(bits, field->type), bits, field->type, FieldStatus::present};
    }

    // Oltre la parte ricevuta. Sotto la lunghezza base il protocollo garantisce
    // che si tratti di zeri rimossi, quindi il valore e' davvero zero: il taglio
    // puo' cadere anche dentro il campo, e completarlo con zeri e' allora
    // corretto. Oltre la lunghezza base quel campo non lo ha scritto nessuno, e
    // non va inventato.
    if (end <= message.base_length) {
        std::array<std::uint8_t, 8> completed{};
        for (std::size_t i = 0; i < element; ++i) {
            const std::size_t position = offset + i;
            completed[i] = position < frame.payload_length
                               ? frame.payload[position]
                               : std::uint8_t{0};
        }
        const std::uint64_t bits = load(completed.data(), element);
        return {as_number(bits, field->type), bits, field->type, FieldStatus::present};
    }
    return {0.0, 0, field->type, FieldStatus::not_transmitted};
}

std::optional<double> read_number(const MessageDefinition& message, const Frame& frame,
                                  std::string_view name, std::size_t index) noexcept {
    const FieldRead field = read_field(message, frame, name, index);
    if (!field.ok()) return std::nullopt;
    return field.value;
}

std::optional<std::int64_t> read_integer(const MessageDefinition& message,
                                         const Frame& frame, std::string_view name,
                                         std::size_t index) noexcept {
    const FieldRead field = read_field(message, frame, name, index);
    if (!field.ok()) return std::nullopt;
    return as_signed(field.bits, field.type);
}

std::optional<std::uint64_t> read_unsigned(const MessageDefinition& message,
                                           const Frame& frame, std::string_view name,
                                           std::size_t index) noexcept {
    const FieldRead field = read_field(message, frame, name, index);
    if (!field.ok()) return std::nullopt;
    return field.bits;
}

std::string read_text(const MessageDefinition& message, const Frame& frame,
                      std::string_view name) {
    const FieldDefinition* field = find_field(message, name);
    if (field == nullptr || field->type != FieldType::ch) return {};

    std::string text;
    for (std::size_t i = 0; i < field->array_length; ++i) {
        const std::size_t offset = field->offset + i;
        if (offset >= frame.payload_length) break;
        const auto character = static_cast<char>(frame.payload[offset]);
        if (character == '\0') break;
        text.push_back(character);
    }
    return text;
}

}  // namespace efis::mavlink
