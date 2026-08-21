#pragma once

#include "efis/core/cross_check.hpp"
#include "efis/core/flight_snapshot.hpp"
#include "efis/core/stasis_monitor.hpp"

#include <chrono>

namespace efis {

// Tarature del modello di volo.
//
// Stato: DEVELOPMENT. Le soglie sono scelte con criterio fisico e documentate
// una per una, ma non sono ancora state validate su registrazioni di volo vere.
// Finche' non lo saranno, servono a far comparire un guasto grossolano, non a
// misurare quanto un sensore stia derivando.
struct FlightModelPolicy final {
    std::chrono::milliseconds stale_after{500};

    // Velocita' al suolo sopra la quale l'aeromobile e' considerato in moto.
    //
    // Sotto questa soglia la sorveglianza della stasi resta sospesa, perche' a
    // terra fermi assetto, quota e velocita' sono legittimamente costanti e
    // ogni soglia produrrebbe un allarme falso a ogni sosta. Il valore e'
    // superiore alla velocita' di rullaggio e inferiore a quella di rotazione:
    // la sorveglianza si apre durante la corsa di decollo, che e' esattamente
    // il momento in cui un pitot ostruito diventa pericoloso.
    double motion_threshold_kt{30.0};

    // Il pitot ostruito e lo statico ostruito sono i guasti che questa
    // sorveglianza esiste per intercettare. In volo velocita' e quota non
    // restano mai costanti entro la risoluzione dello strumento per un tempo
    // simile.
    std::chrono::milliseconds air_data_suspect_after{15000};
    double airspeed_resolution_kt{0.5};
    double altitude_resolution_ft{1.0};

    // L'assetto di un aeromobile in volo oscilla sempre. Un rollio o un
    // beccheggio perfettamente costanti per dieci secondi indicano un inerziale
    // che si e' latchato, non un volo stabile.
    std::chrono::milliseconds attitude_suspect_after{10000};
    double attitude_resolution_deg{0.1};

    std::chrono::milliseconds ground_speed_suspect_after{15000};
    double ground_speed_resolution_kt{0.5};

    // Confronto fra velocita' indicata e velocita' al suolo.
    //
    // Le due differiscono per il vento, quindi la tolleranza deve essere ampia:
    // intercetta il guasto grossolano, per esempio una velocita' indicata ferma
    // mentre quella al suolo cresce, e non la deriva sottile, che richiede un
    // confronto diverso. Una tolleranza stretta qui produrrebbe allarmi a ogni
    // giornata ventosa.
    CrossCheckPolicy speed_agreement{.tolerance = 60.0,
                                     .dwell = std::chrono::milliseconds{5000}};
};

// Normalizza lo stato di volo: invecchiamento, sorveglianza del valore fermo,
// confronto fra catene indipendenti, salute complessiva e allarmi.
//
// Il modello e' ora dotato di memoria, e non puo' piu' essere altrimenti: stasi
// e disaccordo si misurano nel tempo, non su un singolo istantaneo. Per questo
// normalize() non e' const.
class FlightModel final {
public:
    explicit FlightModel(FlightModelPolicy policy = {});

    // Conservato per il codice esistente, che indicava soltanto il tempo di
    // scadenza.
    explicit FlightModel(std::chrono::milliseconds stale_after);

    [[nodiscard]] FlightSnapshot normalize(FlightSnapshot input, MonoTime now);

    // Stato per grandezza della sorveglianza. Serve alla diagnostica e alla
    // black box: la salute nello snapshot dice quanti campi sono fermi, qui c'e'
    // quali e da quanto.
    [[nodiscard]] const StasisMonitor& stasis() const noexcept { return stasis_; }

    [[nodiscard]] const CrossCheckResult& speed_agreement() const noexcept {
        return speed_check_.result();
    }

    [[nodiscard]] const FlightModelPolicy& policy() const noexcept { return policy_; }

private:
    void configure_stasis();

    FlightModelPolicy policy_;
    StasisMonitor stasis_;
    CrossCheck speed_check_;
};

}  // namespace efis
