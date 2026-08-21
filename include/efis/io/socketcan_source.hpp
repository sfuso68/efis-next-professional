#pragma once

#include "efis/io/can_source.hpp"

#include <cstdint>
#include <span>
#include <string>

namespace efis::io {

struct SocketCanStatistics final {
    std::uint64_t frames_read{};

    // Frame di errore del bus: cavo interrotto, nessun riscontro, controllore
    // passivo. Non trasportano misure e non vengono consegnati, ma sono la
    // diagnosi piu' diretta di un collegamento che sta cedendo, quindi vanno
    // contati e mostrati.
    std::uint64_t error_frames{};

    // Richieste remote. Il protocollo del nodo sensori non le usa.
    std::uint64_t remote_frames{};

    // Frame persi dal nucleo perche' la coda del socket si e' riempita prima che
    // li leggessimo. E' un difetto nostro, non del bus: significa che il ciclo
    // di lettura non sta al passo.
    std::uint64_t dropped_by_kernel{};

    std::uint64_t read_errors{};
};

// Legge frame da un'interfaccia SocketCAN del sistema.
//
// Disponibile soltanto su Linux, ed e' il motivo per cui il trasporto sta dietro
// CanSource: su Windows si lavora sulle registrazioni, su Linux sullo stesso
// codice alimentato dal bus vero. `vcan0` sul nodo black box permette di provare
// l'intera catena senza hardware collegato.
//
// Sola lettura, per costruzione: il socket viene aperto e non esiste in questa
// classe alcuna funzione che scriva sul bus.
class SocketCanSource final : public CanSource {
public:
    explicit SocketCanSource(std::string interface_name);
    ~SocketCanSource() override;

    // Apre e collega il socket. Restituisce falso in caso di errore, e in quel
    // caso `error()` dice quale.
    //
    // La distinzione fra "non aperta" e "aperta e silenziosa" non e' formale: un
    // bus scollegato e un bus fermo producono entrambi zero frame, e senza
    // questa differenza il sistema mostrerebbe l'assenza di un'interfaccia come
    // se fosse un nodo che tace.
    [[nodiscard]] bool open();
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept { return descriptor_ >= 0; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] const std::string& interface_name() const noexcept {
        return interface_;
    }

    [[nodiscard]] std::size_t read(std::span<can::Frame> frames) override;

    // Un collegamento vivo non finisce mai. Restituisce vero soltanto se il
    // socket non e' aperto, perche' da una sorgente chiusa non arrivera' altro.
    [[nodiscard]] bool exhausted() const noexcept override { return !is_open(); }

    [[nodiscard]] const SocketCanStatistics& statistics() const noexcept {
        return statistics_;
    }

    // Vero se questa build sa parlare con SocketCAN. Falso altrove, dove open()
    // fallisce dichiarandolo invece di fingere un bus muto.
    [[nodiscard]] static bool supported() noexcept;

private:
    std::string interface_;
    std::string error_;
    int descriptor_{-1};
    SocketCanStatistics statistics_{};
};

}  // namespace efis::io
