#include "efis/core/altimetry.hpp"

#include <cmath>

namespace efis {
namespace {

// Costanti della troposfera standard, quelle su cui e' tarato ogni altimetro
// barometrico. Il coefficiente e' il rapporto fra costante dei gas per il
// gradiente termico e accelerazione di gravita'; la quota di riferimento e' la
// quota alla quale la pressione si annullerebbe se il gradiente continuasse.
constexpr double exponent = 0.190284;
constexpr double scale_ft = 145366.45;

}  // namespace

double pressure_from_altitude_hpa(double pressure_altitude_ft) noexcept {
    const double ratio = 1.0 - pressure_altitude_ft / scale_ft;
    // Sopra la quota di riferimento la formula non ha piu' significato fisico.
    // Non ci si arriva volando, ma un dato corrotto puo' portarci: meglio un
    // valore al limite che una potenza di un numero negativo.
    if (ratio <= 0.0) return 0.0;
    return standard_pressure_hpa * std::pow(ratio, 1.0 / exponent);
}

double altitude_from_pressure_ft(double static_pressure_hpa,
                                 double setting_hpa) noexcept {
    if (static_pressure_hpa <= 0.0 || setting_hpa <= 0.0) return 0.0;
    return scale_ft * (1.0 - std::pow(static_pressure_hpa / setting_hpa, exponent));
}

double indicated_altitude_ft(double pressure_altitude_ft,
                             double setting_hpa) noexcept {
    // Con la regolazione standard la risposta e' la quota di pressione stessa.
    // Il caso viene trattato a parte non per velocita' ma per esattezza: andata
    // e ritorno attraverso pow lascerebbero un residuo di qualche millesimo di
    // piede, e un altimetro che indica 4999,997 con la regolazione standard
    // sarebbe un difetto visibile in un confronto a immagine.
    if (setting_hpa == standard_pressure_hpa) return pressure_altitude_ft;

    const double static_pressure = pressure_from_altitude_hpa(pressure_altitude_ft);
    return altitude_from_pressure_ft(static_pressure, setting_hpa);
}

}  // namespace efis
