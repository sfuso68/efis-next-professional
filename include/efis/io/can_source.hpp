#pragma once

#include "efis/providers/can_frame.hpp"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <vector>

namespace efis::io {

// Sorgente di frame CAN, indipendente da chi li produce.
//
// Stessa separazione di ByteSource e stessa ragione: un bus vero, un bus
// virtuale e una registrazione da rileggere devono alimentare l'adapter senza
// che l'adapter sappia la differenza. E' cio' che rende una prova riproducibile
// e cio' che permette di lavorare senza hardware collegato. Vedi ADR-003.
class CanSource {
public:
    CanSource() = default;
    CanSource(const CanSource&) = delete;
    CanSource& operator=(const CanSource&) = delete;
    CanSource(CanSource&&) = delete;
    CanSource& operator=(CanSource&&) = delete;
    virtual ~CanSource() = default;

    // Legge fino a frames.size() frame e restituisce quanti ne ha letti.
    //
    // Zero significa "per ora niente", non "finito". Un bus silenzioso e un bus
    // scollegato restituiscono lo stesso valore, e distinguerli non e' compito
    // di questa funzione: lo fanno la sorveglianza dei nodi, che vede il
    // silenzio, e lo stato di apertura della sorgente, che vede l'assenza.
    [[nodiscard]] virtual std::size_t read(std::span<can::Frame> frames) = 0;

    // Vero quando la sorgente non produrra' piu' nulla. Una registrazione
    // finisce; un bus vivo no.
    [[nodiscard]] virtual bool exhausted() const noexcept = 0;
};

// Registrazione di frame, riavvolgibile.
class RecordedCanSource final : public CanSource {
public:
    explicit RecordedCanSource(std::vector<can::Frame> frames);

    [[nodiscard]] std::size_t read(std::span<can::Frame> frames) override;
    [[nodiscard]] bool exhausted() const noexcept override;

    void rewind() noexcept { position_ = 0; }
    [[nodiscard]] std::size_t size() const noexcept { return frames_.size(); }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

private:
    std::vector<can::Frame> frames_;
    std::size_t position_{};
};

// --- Registrazioni in formato candump ---------------------------------------

struct CandumpLog final {
    std::vector<can::Frame> frames;

    // Righe che non si sono potute interpretare. Vanno contate e dichiarate:
    // una registrazione troncata dalla perdita di alimentazione finisce a meta'
    // riga, ed e' un caso atteso, non un motivo per arrotondare il conto.
    std::uint64_t malformed_lines{};

    // Frame con il bit di errore o di richiesta remota. Non trasportano misure e
    // non vengono consegnati: interpretarli come dati significherebbe leggere
    // uno stato del bus come se fosse una grandezza fisica.
    std::uint64_t error_frames{};
    std::uint64_t remote_frames{};
};

// Interpreta il formato prodotto da `candump -l`, che e' lo strumento con cui si
// registra un bus reale:
//
//   (1755739800.123456) can0 100#0102030405060708
//
// Sceglierlo invece di un formato nostro non e' comodita': significa che una
// registrazione fatta sul banco con gli strumenti standard si rilegge qui senza
// conversioni, e che le nostre registrazioni si rileggono con quelli.
[[nodiscard]] CandumpLog parse_candump(std::istream& input);

}  // namespace efis::io
