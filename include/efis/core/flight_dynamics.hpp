#pragma once

namespace efis {

// Relazioni di volo che legano fra loro grandezze gia' misurate.
//
// Stanno nel core e non nel simulatore perche' non servono solo a generare dati
// finti: la stessa relazione che permette al simulatore di produrre un rateo di
// virata coerente permettera' al confronto incrociato di verificare che il
// rateo misurato dal Pixhawk sia compatibile con inclinazione e velocita'. Un
// giroscopio che si blocca continua a dichiarare un rateo, e il modo per
// accorgersene e' sapere quale rateo dovrebbe avere.

inline constexpr double standard_gravity_m_s2 = 9.80665;
inline constexpr double metres_per_second_per_knot = 0.514444;

// Rateo di virata di una virata corretta, in gradi al secondo.
//
// In volo corretto la componente orizzontale della portanza fornisce tutta la
// forza centripeta, e da li' segue omega = g tan(phi) / V. Non e'
// un'approssimazione comoda: e' la fisica della virata, e vale per qualunque
// aeromobile perche' non contiene nulla che dipenda dalla macchina.
//
// A velocita' nulla la relazione diverge: un aeromobile fermo inclinato non sta
// virando, sta cadendo. Restituisce zero.
[[nodiscard]] double coordinated_turn_rate_deg_s(double bank_deg,
                                                 double true_airspeed_kt) noexcept;

// Inclinazione che produce un dato rateo di virata alla velocita' indicata.
// E' l'inversa della precedente e serve a impostare una virata, non a
// descriverla.
[[nodiscard]] double bank_for_turn_rate_deg(double turn_rate_deg_s,
                                            double true_airspeed_kt) noexcept;

// Il rateo che divide le lancette del virosbandometro: tre gradi al secondo,
// cioe' un giro completo in due minuti. E' la virata di riferimento della
// navigazione strumentale, ed e' il motivo per cui il quadrante ha due tacche.
inline constexpr double standard_rate_deg_s = 3.0;

}  // namespace efis
