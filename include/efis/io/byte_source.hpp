#pragma once

#include "efis/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace efis::io {

// Sorgente di byte, indipendente dal protocollo che trasportano.
//
// Non sa se veicola MAVLink, CAN o altro: muove byte. Vedi ADR-003, che vieta
// al trasporto di conoscere i protocolli e agli adapter di conoscere il
// trasporto.
class ByteSource {
public:
    ByteSource() = default;
    ByteSource(const ByteSource&) = delete;
    ByteSource& operator=(const ByteSource&) = delete;
    ByteSource(ByteSource&&) = delete;
    ByteSource& operator=(ByteSource&&) = delete;
    virtual ~ByteSource() = default;

    // Legge fino a buffer.size() byte e restituisce quanti ne ha letti.
    //
    // Zero significa "per ora niente", non "finito": una seriale silenziosa e
    // una seriale scollegata restituiscono lo stesso valore, e distinguerle e'
    // compito di exhausted(), non di questa funzione.
    [[nodiscard]] virtual std::size_t read(std::span<std::uint8_t> buffer) = 0;

    // Vero quando la sorgente non produrra' piu' nulla. Una registrazione
    // finisce; un collegamento vivo no.
    [[nodiscard]] virtual bool exhausted() const noexcept = 0;
};

// Replay da registrazione.
//
// Non e' uno strumento di prova: e' un trasporto di prima classe, richiesto dal
// paragrafo 17.3 dell'handoff. Una registrazione reale rende riproducibile ogni
// prova successiva, cosa che un collegamento fisico non fa.
//
// La consegna avviene a blocchi di dimensione fissa perche' una seriale non
// consegna mai i dati allineati ai confini dei messaggi: un replay che
// restituisse tutto in una volta non eserciterebbe la frammentazione, che e' il
// caso normale e non l'eccezione.
class RecordedByteSource final : public ByteSource {
public:
    // Legge l'intera registrazione in memoria. Le registrazioni di riferimento
    // sono piccole e devono poter essere riavvolte senza rileggere il disco.
    explicit RecordedByteSource(const std::filesystem::path& path,
                                std::size_t chunk = 512);

    explicit RecordedByteSource(std::vector<std::uint8_t> bytes,
                                std::size_t chunk = 512);

    [[nodiscard]] std::size_t read(std::span<std::uint8_t> buffer) override;
    [[nodiscard]] bool exhausted() const noexcept override;

    // Riporta la lettura all'inizio. Due riproduzioni della stessa
    // registrazione devono produrre gli stessi risultati: e' cio' che rende il
    // replay deterministico e quindi utile come banco.
    void rewind() noexcept { position_ = 0; }

    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t chunk_;
    std::size_t position_{};
};

// Trasferisce byte dalla sorgente al consumatore finche' ce ne sono, e
// restituisce quanti ne ha trasferiti.
//
// E' un template di proposito. Il trasporto non conosce il consumatore e il
// consumatore non conosce il trasporto: a unirli e' chi li usa, non una
// dipendenza scritta in una delle due parti. E' il modo in cui l'ADR-003 impedisce
// che il trasporto finisca per sapere che cosa trasporta.
//
// `sink` deve esporre `consume(std::span<const std::uint8_t>, MonoTime)`.
template <class Sink>
std::size_t pump(ByteSource& source, Sink& sink, MonoTime now,
                 std::size_t buffer_size = 512) {
    std::vector<std::uint8_t> buffer(buffer_size == 0 ? 1 : buffer_size);
    std::size_t moved = 0;
    while (!source.exhausted()) {
        const std::size_t count = source.read(buffer);
        // Una sorgente non esaurita che non ha nulla da dare adesso: si esce, e
        // si ritornera' quando ne avra'. Insistere qui bloccherebbe il ciclo di
        // volo su una seriale silenziosa.
        if (count == 0) break;
        sink.consume(std::span<const std::uint8_t>(buffer.data(), count), now);
        moved += count;
    }
    return moved;
}

}  // namespace efis::io
