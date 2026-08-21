#pragma once

#include "efis/core/sample.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

namespace efis {

// Confronto fra grandezze ottenute per vie indipendenti.
//
// `Validity::disagree` esisteva nel modello dati fin dall'inizio e non lo
// produceva nessuno: nessun confronto incrociato era implementato. Un solo
// sensore che sbaglia in modo coerente resta quindi invisibile, perche' non c'e'
// niente con cui contraddirlo.
//
// Il principio e' che due catene fisicamente separate non si guastano allo
// stesso modo nello stesso istante. La quota barometrica dipende da una capsula
// di pressione, quella satellitare da una costellazione: se le loro variazioni
// divergono, una delle due sta mentendo, e l'informazione utile e' proprio che
// non si sa quale.

enum class Agreement : std::uint8_t {
    // Manca un termine di paragone: uno dei due valori non e' utilizzabile,
    // oppure non sono ancora trascorse abbastanza osservazioni. Non e' accordo.
    unknown,

    agree,

    // Lo scostamento supera la tolleranza da piu' del tempo di persistenza.
    disagree
};

[[nodiscard]] constexpr const char* agreement_name(Agreement value) noexcept {
    switch (value) {
        case Agreement::unknown: return "UNKNOWN";
        case Agreement::agree: return "AGREE";
        case Agreement::disagree: return "DISAGREE";
    }
    return "UNKNOWN";
}

struct CrossCheckPolicy final {
    // Scostamento oltre il quale le due grandezze sono considerate in
    // disaccordo, nell'unita' delle grandezze confrontate.
    double tolerance{0.0};

    // Per quanto lo scostamento deve persistere prima di essere dichiarato.
    //
    // Non e' un dettaglio di comodo: raffiche, ritardi di filtro e tempi di
    // campionamento diversi producono scostamenti istantanei del tutto normali.
    // Dichiarare il disaccordo al primo campione riempirebbe lo strumento di
    // allarmi falsi, e un allarme che grida sempre e' un allarme che nessuno
    // guarda piu'.
    std::chrono::milliseconds dwell{3000};
};

struct CrossCheckResult final {
    Agreement agreement{Agreement::unknown};

    // Scostamento osservato all'ultimo confronto.
    double difference{};

    // Da quanto tempo lo scostamento supera la tolleranza.
    std::chrono::milliseconds exceeded_for{};
};

// Confronta due grandezze che devono muoversi insieme.
//
// Mantiene lo stato necessario a misurare la persistenza. Un'istanza per
// confronto: velocita' verticale barometrica contro satellitare, velocita'
// indicata contro velocita' al suolo, e cosi' via.
class CrossCheck final {
public:
    explicit CrossCheck(CrossCheckPolicy policy) noexcept;

    // Confronta due campioni osservati nello stesso istante.
    //
    // Se anche uno solo dei due non e' utilizzabile il risultato e' `unknown` e
    // la persistenza si azzera: confrontare con un dato scaduto produrrebbe un
    // disaccordo che dipende dal guasto gia' noto, non da una discrepanza vera.
    CrossCheckResult compare(const Sample<double>& first,
                             const Sample<double>& second,
                             MonoTime now);

    [[nodiscard]] const CrossCheckResult& result() const noexcept { return result_; }

    void reset() noexcept;

private:
    CrossCheckPolicy policy_;
    CrossCheckResult result_{};
    MonoTime exceeded_since_{};
    bool exceeding_{};
};

// Applica l'esito a due campioni.
//
// In caso di disaccordo entrambi diventano `Validity::disagree`, non uno solo:
// il confronto dice che le due catene non concordano, non quale delle due abbia
// torto. Sceglierne una da marcare significherebbe attribuire una colpa che il
// confronto non e' in grado di stabilire, e presentare l'altra come sana.
void apply_disagreement(const CrossCheckResult& result,
                        Sample<double>& first,
                        Sample<double>& second) noexcept;

}  // namespace efis
