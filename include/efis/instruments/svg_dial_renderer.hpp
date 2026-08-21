#pragma once

#include "efis/instruments/dial.hpp"
#include "efis/instruments/horizon.hpp"

#include <string>

namespace efis::instruments {

// Disegna un quadrante in SVG.
//
// Non e' il renderer operativo: quello sara' un nodo scene graph di Qt, e
// disegnera' la stessa geometria. Questo serve a due cose che il renderer
// operativo non puo' fare. Produce le immagini di confronto per le prove, senza
// bisogno di una scheda grafica e quindi anche dove i binari non si possono
// eseguire. E permette di guardare uno strumento mentre lo si sta ancora
// scrivendo, che e' l'unico modo per accorgersi che un quadrante corretto e'
// illeggibile.
//
// Se le due rappresentazioni divergessero, il confronto a immagine non
// direbbe piu' nulla sullo strumento vero: e' per questo che la geometria sta
// nel core e nessuno dei due renderer la ricalcola.

struct DialStyle final {
    // Titolo sopra il quadrante, per esempio «IAS» o «RPM».
    std::string title;

    // Unita' mostrata sotto il centro.
    std::string unit;

    // Lato dell'immagine in pixel. Il quadrante ne occupa la parte interna.
    double size_px{480.0};
};

[[nodiscard]] std::string render_svg(const Dial& dial, const DialStyle& style);

[[nodiscard]] std::string render_svg(const Horizon& horizon, const DialStyle& style);

[[nodiscard]] std::string render_svg(const TurnCoordinator& coordinator,
                                     const DialStyle& style);

}  // namespace efis::instruments
