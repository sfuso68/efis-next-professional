#pragma once

#include "efis/fdm/dynamics.hpp"

namespace efis::fdm {

// Pilota di scenario: tiene il modello su un profilo di volo.
//
// Che cosa non e'. Non e' un pilota automatico, e non deve diventarlo: non
// legge i sensori, legge la verita' del modello, e scrive sui comandi del
// modello e non su un bus. Il confine di sicurezza del simulatore di sistema
// vale anche qui -- nessun percorso di comando verso l'aeromobile -- e questo
// codice lo rispetta per costruzione, perche' non ha modo di uscire dal
// processo.
//
// Perche' serve. Un aeroplano lasciato a se' stesso con i comandi fissi non
// vola livellato: ha un modo di oscillazione lento, il fugoide, con periodo di
// decine di secondi, e dopo pochi minuti la quota e' un'altra. E' corretto che
// sia cosi', perche' gli aeroplani veri fanno questo. Ma una prova di dieci
// minuti in crociera deve restare in crociera, altrimenti verifica la deriva
// del modello e non il comportamento dell'EFIS. Il pilota di scenario e' la
// mano che un pilota vero tiene sui comandi.
//
// Leggendo la verita' e non i sensori si ottiene anche una proprieta' che serve
// alle prove di guasto: il profilo resta lo stesso mentre il sensore mente. Un
// Pitot ostruito non deve far cambiare traiettoria all'aeroplano, altrimenti
// non si capisce piu' se cio' che si vede sul display venga dal guasto o dalla
// reazione al guasto.

struct PilotTargets final {
    double altitude_ft{3000.0};
    double heading_deg{0.0};
    double airspeed_kcas{90.0};
    double flap_deg{0.0};
    bool gear_down{true};

    // Rateo di salita massimo consentito al pilota, in piedi al minuto. Serve
    // a far salire l'aeroplano come lo farebbe salire una persona, e non con
    // la cabrata massima che l'equilibratore consentirebbe.
    double max_vertical_speed_fpm{700.0};

    // Vero durante la corsa di decollo e l'atterraggio: il pilota tiene l'asse
    // pista con il rudder e non inclina. Un controllo di prua che agisse
    // sull'alettone con il carrello a terra farebbe strisciare l'ala.
    bool on_runway{false};

    // Vero quando il banco vuole portare l'aeroplano allo stallo. Non sospende
    // il controllo della quota: lo usa. A motore ridotto e quota tenuta la
    // velocita' decade e l'incidenza sale fino al cedimento, che e' come la
    // prova di stallo si conduce davvero. Toglie soltanto la potenza.
    bool stall_entry{false};
};

class ScenarioPilot final {
public:
    explicit ScenarioPilot(const Airframe& airframe) noexcept;

    void set_targets(const PilotTargets& targets) noexcept { targets_ = targets; }
    [[nodiscard]] const PilotTargets& targets() const noexcept { return targets_; }

    // Agisce sui comandi del veicolo per un passo. Non integra la dinamica: il
    // ciclo resta di chi lo possiede.
    void fly(Vehicle& vehicle, double dt_s) noexcept;

private:
    const Airframe* airframe_;
    PilotTargets targets_{};
    double throttle_{};
    double elevator_trim_{};
    double pitch_integral_{};

    // Falso fino alla prima chiamata: manetta e trim si prendono da dove
    // l'aeroplano e' stato posato, non da zero.
    bool initialised_{};

};

}  // namespace efis::fdm
