#pragma once

#include "efis/instruments/dial.hpp"

#include <string>
#include <vector>

namespace efis::instruments {

// Orizzonte artificiale e virosbandometro.
//
// Non sono quadranti a lancetta e hanno percio' una geometria propria. Sul
// primo si muove tutto il quadro, non un indice; sul secondo l'indice e' la
// sagoma di un aeroplano, e accanto c'e' una pallina che non misura la stessa
// grandezza.

// --- Orizzonte artificiale ---------------------------------------------------
//
// Il quadro ruota in senso opposto al rollio e trasla verticalmente col
// beccheggio: e' il mondo a inclinarsi, non l'aeroplano. Invertire uno dei due
// versi produce uno strumento che si legge benissimo e mostra l'assetto
// specchiato, che e' l'errore che non salta all'occhio.
//
// Quando l'assetto manca il quadro non viene disegnato. Un orizzonte fermo e'
// la riproduzione fedele di un indicatore guasto, e si legge come volo livellato
// e ali orizzontali: e' la condizione che ha ucciso piu' piloti di qualunque
// altra lettura errata, e non va imitata.

// Gradino della scala di beccheggio: una barretta orizzontale nel quadro, con la
// propria distanza dall'orizzonte.
struct Rung final {
    // Beccheggio a cui corrisponde, in gradi. Negativo sotto l'orizzonte.
    double pitch_deg{};

    // Distanza dall'orizzonte in frazione di raggio, positiva verso l'alto.
    double offset{};

    // Semilunghezza della barretta, in frazione di raggio. I gradini principali
    // sono piu' lunghi, cosi' si contano senza leggerli.
    double half_width{};

    // Vuoto sui gradini intermedi, che non portano scritta.
    std::string text;
};

struct Horizon final {
    // Rotazione del quadro, gia' invertita rispetto al rollio.
    double rotation_deg{};

    // Traslazione verticale del quadro in frazione di raggio, positiva quando
    // l'orizzonte scende, cioe' a cabrare.
    double translation{};

    std::vector<Rung> ladder;

    // Tacche della scala di inclinazione, fisse sulla ghiera: non ruotano col
    // quadro, perche' e' rispetto a loro che si legge il rollio.
    std::vector<Tick> bank_scale;

    // Posizione dell'indice di rollio sulla scala.
    double bank_pointer_deg{};

    // Falso quando non c'e' assetto da mostrare: niente quadro, niente indice.
    bool has_attitude{};

    Annunciation annunciation{Annunciation::none};
    bool simulated{};
};

struct AttitudeReading final {
    Reading roll;
    Reading pitch;
};

[[nodiscard]] Horizon describe_attitude(const AttitudeReading& reading,
                                        const Sizing& sizing);

// --- Virosbandometro ---------------------------------------------------------
//
// Due grandezze indipendenti sullo stesso strumento, ed e' proprio la loro
// indipendenza a renderlo utile: la sagoma mostra quanto si sta virando, la
// pallina se la virata e' corretta. Una virata puo' essere giusta e scoordinata,
// o coordinata e sbagliata, e servono entrambe le informazioni per distinguerle.
//
// Le due possono guastarsi separatamente, e vanno percio' dichiarate separate:
// una sagoma assente con la pallina presente e' uno stato legittimo.
struct TurnReading final {
    // Rateo di virata in gradi al secondo.
    Reading turn_rate;

    // Accelerazione laterale in multipli di gravita'.
    Reading slip;
};

struct TurnCoordinator final {
    // Inclinazione della sagoma dell'aeroplano, proporzionale al rateo.
    double aircraft_bank_deg{};
    bool has_turn{};

    // Spostamento della pallina lungo il tubo, da -1 a 1.
    double ball_offset{};
    bool has_ball{};

    // Tacche della virata standard, a destra e a sinistra: sono il riferimento
    // per cui il quadrante esiste.
    std::vector<Tick> standard_marks;

    Annunciation annunciation{Annunciation::none};
    bool simulated{};
};

[[nodiscard]] TurnCoordinator describe_turn(const TurnReading& reading,
                                            const Sizing& sizing);

}  // namespace efis::instruments
