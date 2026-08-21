#pragma once

namespace efis {

// Conversioni fra pressione e quota nell'atmosfera standard.
//
// La quota di pressione e' quella che un altimetro indica con la regolazione a
// 1013,25 hPa. La quota indicata e' quella che indica con la regolazione
// effettivamente impostata dal pilota. Le due coincidono solo quando la
// regolazione e' quella standard, e tenerle distinte non e' pedanteria: due
// voli registrati con regolazioni diverse avrebbero quote non confrontabili se
// nella black box ne finisse una sola senza sapere quale correzione fosse
// applicata.
//
// Il calcolo passa dalla formula barometrica invece che dalla regola pratica
// dei ventisette piedi per ettopascal. Quella regola vale vicino al livello del
// mare e sbaglia progressivamente salendo, e un altimetro che sbaglia
// progressivamente e' peggio di uno che sbaglia sempre uguale: l'errore cresce
// proprio dove i margini si riducono.

inline constexpr double standard_pressure_hpa = 1013.25;

// La regolazione piu' bassa e piu' alta che una finestra di Kollsman accetta.
// Fuori da questo intervallo non si tratta di una pressione atmosferica ma di
// un errore di inserimento, e va rifiutato invece che applicato.
inline constexpr double minimum_setting_hpa = 850.0;
inline constexpr double maximum_setting_hpa = 1100.0;

[[nodiscard]] constexpr bool is_plausible_setting(double setting_hpa) noexcept {
    return setting_hpa >= minimum_setting_hpa && setting_hpa <= maximum_setting_hpa;
}

// Pressione statica corrispondente a una quota di pressione.
[[nodiscard]] double pressure_from_altitude_hpa(double pressure_altitude_ft) noexcept;

// Quota indicata da un altimetro regolato su `setting_hpa`, data la pressione
// statica.
[[nodiscard]] double altitude_from_pressure_ft(double static_pressure_hpa,
                                               double setting_hpa) noexcept;

// Quota indicata a partire dalla quota di pressione e dalla regolazione.
//
// Con la regolazione standard restituisce esattamente la quota di pressione:
// non approssimativamente, esattamente, perche' il passaggio inverso e quello
// diretto usano la stessa formula.
[[nodiscard]] double indicated_altitude_ft(double pressure_altitude_ft,
                                           double setting_hpa) noexcept;

// Conversioni fra le due unita' in cui si esprime la regolazione. Gli altimetri
// europei sono in ettopascal, quelli americani in pollici di mercurio, e il
// profilo aeromobile dichiara quale usare.
inline constexpr double hectopascal_per_inch_mercury = 33.8638866667;

[[nodiscard]] constexpr double inches_to_hectopascal(double inches) noexcept {
    return inches * hectopascal_per_inch_mercury;
}

[[nodiscard]] constexpr double hectopascal_to_inches(double hectopascal) noexcept {
    return hectopascal / hectopascal_per_inch_mercury;
}

}  // namespace efis
