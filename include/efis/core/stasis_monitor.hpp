#pragma once

#include "efis/core/sample.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

namespace efis {

// Rilevamento del valore fermo.
//
// Il sistema sa gia' accorgersi di un nodo muto, di un nodo bloccato che ripete
// la stessa sequenza e di un campione non aggiornato entro il tempo previsto.
// Resta scoperto il caso peggiore: un nodo che trasmette regolarmente, con
// sequenza crescente e checksum valido, portando un valore che non cambia piu'
// perche' il sensore dietro e' morto.
//
// In quella condizione `received_at` continua ad aggiornarsi, il campione resta
// valido per sempre, e lo strumento mostra un numero fermo con l'aria di essere
// corretto. E' il pitot ostruito, l'inerziale che si latcha, il barometro che si
// pianta: la famiglia di guasti piu' pericolosa, perche' lo strumento non dice
// "non lo so", dice un numero.

struct StasisPolicy final {
    // Oltre questo tempo senza variazioni apprezzabili il valore e' sospetto.
    //
    // Va scelto per segnale, non una volta per tutte: l'assetto di un aeroplano
    // in volo non e' mai perfettamente costante, mentre una quota selezionata
    // dal pilota lo e' legittimamente per tutto il volo.
    std::chrono::milliseconds suspect_after{5000};

    // Variazione minima da considerare movimento.
    //
    // Non si confronta per uguaglianza esatta: un sensore quantizzato a un piede
    // riporta piu' campioni identici di seguito senza essere guasto. La soglia
    // e' la risoluzione dichiarata dello strumento, non lo zero.
    double resolution{0.0};

    // Alcune grandezze possono restare ferme a lungo senza che significhi nulla:
    // una quota selezionata, un corso impostato, un contatore. Per quelle il
    // rilevamento va disattivato invece che tarato, perche' una soglia molto
    // alta e' comunque una soglia sbagliata.
    bool applicable{true};
};

enum class Motion : std::uint8_t {
    // Non ci sono ancora abbastanza osservazioni per dire qualcosa.
    unknown,

    // Il valore si muove.
    moving,

    // Il valore e' fermo da piu' del tempo consentito.
    //
    // Non e' una diagnosi di guasto: e' un'osservazione. Puo' darsi che il volo
    // sia davvero stabile. Per questo lo stato si chiama "sospetto" e non
    // "guasto", e la reazione corretta e' segnalarlo al pilota, non sostituire
    // il valore ne' nasconderlo.
    suspect
};

[[nodiscard]] constexpr const char* motion_name(Motion motion) noexcept {
    switch (motion) {
        case Motion::unknown: return "UNKNOWN";
        case Motion::moving: return "MOVING";
        case Motion::suspect: return "SUSPECT";
    }
    return "UNKNOWN";
}

struct StasisState final {
    SignalId signal{SignalId::unknown};
    Motion motion{Motion::unknown};

    // Ultimo valore che ha costituito un movimento apprezzabile, e quando.
    double reference{};
    MonoTime moved_at{};

    // Per quanto tempo il valore e' rimasto fermo, all'ultima osservazione.
    std::chrono::milliseconds still_for{};
};

// Sorveglia una o piu' grandezze e dichiara quando smettono di muoversi.
//
// Il monitor osserva soltanto: non modifica i campioni e non decide che cosa
// mostrare. La reazione appartiene a chi compone lo stato di volo, perche' la
// scelta fra segnalare, degradare o allarmare dipende dalla grandezza e non dal
// meccanismo di rilevamento.
class StasisMonitor final {
public:
    // Registra la politica per un segnale. Un segnale senza politica non viene
    // sorvegliato: e' preferibile non dire nulla piuttosto che applicare una
    // soglia inventata.
    void configure(SignalId signal, StasisPolicy policy);

    // Osserva un campione e restituisce lo stato del movimento.
    //
    // Un campione non utilizzabile azzera la sorveglianza invece di alimentarla:
    // su un dato gia' dichiarato scaduto o assente la stasi non significa nulla,
    // e contarla produrrebbe due allarmi per un solo guasto.
    Motion observe(const Sample<double>& sample, MonoTime now);

    [[nodiscard]] Motion motion(SignalId signal) const noexcept;
    [[nodiscard]] const std::vector<StasisState>& states() const noexcept {
        return states_;
    }

    void reset() noexcept;

private:
    struct Entry final {
        SignalId signal{SignalId::unknown};
        StasisPolicy policy{};
        StasisState state{};
        bool seen{};
    };

    [[nodiscard]] Entry* find(SignalId signal) noexcept;

    std::vector<Entry> entries_;
    std::vector<StasisState> states_;
};

}  // namespace efis
