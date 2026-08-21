#pragma once

#include "efis/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace efis::io {

// Un frame CAN 2.0A.
//
// Il CAN non e' un flusso di byte ma una sequenza di messaggi delimitati
// dall'hardware: non esiste risincronizzazione da fare e non esiste frame a
// meta'. Per questo il trasporto CAN ha un'interfaccia propria invece di
// riusare ByteSource, che risolve un problema che qui non si pone.
struct CanFrame final {
    // Identificatore standard a 11 bit. Gli identificatori estesi non sono
    // previsti da Engine CAN V1.
    std::uint16_t id{};

    std::uint8_t length{};
    std::array<std::uint8_t, 8> data{};

    [[nodiscard]] constexpr std::uint8_t node() const noexcept {
        return static_cast<std::uint8_t>((id >> 7) & 0x0F);
    }
    [[nodiscard]] constexpr std::uint8_t message() const noexcept {
        return static_cast<std::uint8_t>(id & 0x7F);
    }
};

class CanSource {
public:
    CanSource() = default;
    CanSource(const CanSource&) = delete;
    CanSource& operator=(const CanSource&) = delete;
    CanSource(CanSource&&) = delete;
    CanSource& operator=(CanSource&&) = delete;
    virtual ~CanSource() = default;

    // Prossimo frame disponibile, se c'e'. Nessun valore significa "per ora
    // niente", non "finito": distinguerli e' compito di exhausted().
    [[nodiscard]] virtual std::optional<CanFrame> next() = 0;

    [[nodiscard]] virtual bool exhausted() const noexcept = 0;
};

// Replay da una cattura in formato candump.
//
// E' lo stesso formato che `candump -L` produce sul nodo black box, quindi una
// registrazione presa dal bus vero si rigioca qui senza conversioni. Come per il
// trasporto a byte, il replay non e' uno strumento di prova aggiunto in coda: e'
// cio' che rende riproducibile una prova che sul bus non lo sarebbe.
//
// Formato di una riga: `(1755772800.123456) vcan0 081#0C6009B60302`
class RecordedCanSource final : public CanSource {
public:
    explicit RecordedCanSource(const std::filesystem::path& path);
    explicit RecordedCanSource(std::vector<std::string> lines);

    [[nodiscard]] std::optional<CanFrame> next() override;
    [[nodiscard]] bool exhausted() const noexcept override;

    void rewind() noexcept { position_ = 0; }

    // Righe che non rispettano il formato. Sono contate e non interrompono la
    // lettura: una cattura puo' contenere commenti o righe troncate.
    [[nodiscard]] std::size_t malformed_lines() const noexcept { return malformed_; }
    [[nodiscard]] std::size_t size() const noexcept { return lines_.size(); }

private:
    std::vector<std::string> lines_;
    std::size_t position_{};
    std::size_t malformed_{};
};

// Interpreta una riga candump. Restituisce nullopt se la riga non e' un frame.
[[nodiscard]] std::optional<CanFrame> parse_candump_line(std::string_view line) noexcept;

}  // namespace efis::io
