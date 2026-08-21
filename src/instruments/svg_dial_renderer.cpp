#include "efis/instruments/svg_dial_renderer.hpp"

#include <iomanip>
#include <sstream>

namespace efis::instruments {
namespace {

// Colori del design system. Il rosso e l'ambra restano esclusivi degli allarmi:
// non compaiono altrove sul quadrante, cosi' quando compaiono significano
// qualcosa.
constexpr const char* background = "#071019";
constexpr const char* face = "#0B1722";
constexpr const char* bezel = "#314654";
constexpr const char* primary = "#F4F7FA";
constexpr const char* secondary = "#9AAEBD";
constexpr const char* cyan = "#00C8F0";
constexpr const char* green = "#58D330";
constexpr const char* amber = "#FFB000";
constexpr const char* warning = "#FF3B30";
constexpr const char* sky = "#087AC1";
constexpr const char* earth = "#795126";

const char* band_colour(Band band) noexcept {
    switch (band) {
        case Band::white: return primary;
        case Band::green: return green;
        case Band::yellow: return amber;
        case Band::red: return warning;
    }
    return primary;
}

std::string number(double value, int decimals = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}

// Settore anulare fra due angoli. Il flag di arco largo serve oltre i
// centottanta gradi, altrimenti SVG disegna il complementare e la fascia
// comparirebbe dalla parte opposta del quadrante.
std::string annulus(double centre, double radius, double start_deg, double end_deg,
                    double inner, double outer) {
    const Point outer_start = polar(start_deg, radius * outer);
    const Point outer_end = polar(end_deg, radius * outer);
    const Point inner_end = polar(end_deg, radius * inner);
    const Point inner_start = polar(start_deg, radius * inner);
    const bool large = (end_deg - start_deg) > 180.0;

    std::ostringstream path;
    path << "M " << number(centre + outer_start.x) << " " << number(centre + outer_start.y)
         << " A " << number(radius * outer) << " " << number(radius * outer)
         << " 0 " << (large ? 1 : 0) << " 1 "
         << number(centre + outer_end.x) << " " << number(centre + outer_end.y)
         << " L " << number(centre + inner_end.x) << " " << number(centre + inner_end.y)
         << " A " << number(radius * inner) << " " << number(radius * inner)
         << " 0 " << (large ? 1 : 0) << " 0 "
         << number(centre + inner_start.x) << " " << number(centre + inner_start.y)
         << " Z";
    return path.str();
}

// Dicitura di stato e marcatura di simulazione: identiche su ogni strumento,
// perche' significano la stessa cosa ovunque compaiano.
std::string annunciation_markup(Annunciation annunciation, bool simulated, double size,
                                double centre, double radius) {
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    if (annunciation != Annunciation::none) {
        const char* colour = annunciation == Annunciation::failed ? warning : amber;
        svg << "<text x='" << centre << "' y='" << number(centre + radius * 0.72)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.15) << "' font-weight='700' fill='" << colour << "'>"
            << annunciation_text(annunciation) << "</text>";
    }
    if (simulated) {
        const double inset = size * 0.02;
        svg << "<rect x='" << number(inset) << "' y='" << number(inset) << "' width='"
            << number(size - 2 * inset) << "' height='" << number(size - 2 * inset)
            << "' fill='none' stroke='" << amber << "' stroke-width='"
            << number(size * 0.012) << "' rx='" << number(size * 0.02) << "'/>";
        svg << "<text x='" << centre << "' y='" << number(size - inset * 2.2)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.12) << "' font-weight='700' fill='" << amber
            << "'>SIMULAZIONE</text>";
    }
    return svg.str();
}

std::string title_markup(const DialStyle& style, double size, double centre, double radius) {
    static_cast<void>(size);
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    if (!style.title.empty()) {
        svg << "<text x='" << centre << "' y='" << number(centre - radius * 0.62)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.13) << "' font-weight='700' fill='" << cyan << "'>"
            << style.title << "</text>";
    }
    return svg.str();
}

}  // namespace

std::string render_svg(const Dial& dial, const DialStyle& style) {
    const double size = style.size_px;
    const double centre = size / 2.0;
    // Il quadrante lascia un margine alla ghiera e alle scritte di contorno.
    const double radius = size * 0.42;

    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << size << "' height='" << size
        << "' viewBox='0 0 " << size << " " << size << "'>";
    svg << "<rect width='" << size << "' height='" << size << "' fill='" << background << "'/>";

    // Ghiera e fondo.
    svg << "<circle cx='" << centre << "' cy='" << centre << "' r='" << radius * 1.06
        << "' fill='" << face << "' stroke='" << bezel << "' stroke-width='"
        << number(size * 0.006) << "'/>";

    // Fasce, sotto tutto il resto.
    for (const Arc& arc : dial.arcs) {
        svg << "<path d='"
            << annulus(centre, radius, arc.start_deg, arc.end_deg,
                       arc.inner_radius, arc.outer_radius)
            << "' fill='" << band_colour(arc.band) << "' fill-opacity='0.85'/>";
    }

    // Tacche.
    for (const Tick& tick : dial.ticks) {
        const Point from = polar(tick.angle_deg, radius * tick.inner_radius);
        const Point to = polar(tick.angle_deg, radius * tick.outer_radius);
        const double width = size * (tick.kind == TickKind::major ? 0.010 : 0.005);
        svg << "<line x1='" << number(centre + from.x) << "' y1='" << number(centre + from.y)
            << "' x2='" << number(centre + to.x) << "' y2='" << number(centre + to.y)
            << "' stroke='" << primary << "' stroke-width='" << number(width)
            << "' stroke-linecap='round'/>";
    }

    // Scritte della scala.
    for (const Label& label : dial.labels) {
        const Point at = polar(label.angle_deg, radius * label.radius);
        const double height = radius * label.height;
        svg << "<text x='" << number(centre + at.x) << "' y='"
            << number(centre + at.y + height * 0.36)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(height) << "' font-weight='700' fill='" << primary << "'>"
            << label.text << "</text>";
    }

    // Titolo e unita'.
    if (!style.title.empty()) {
        svg << "<text x='" << centre << "' y='" << number(centre - radius * 0.38)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.14) << "' font-weight='700' fill='" << cyan << "'>"
            << style.title << "</text>";
    }
    if (!style.unit.empty()) {
        svg << "<text x='" << centre << "' y='" << number(centre + radius * 0.50)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.11) << "' fill='" << secondary << "'>"
            << style.unit << "</text>";
    }

    // Finestrella, sotto le lancette perche' non deve coprirle.
    if (dial.window.present) {
        const Point at = polar(dial.window.angle_deg, radius * dial.window.radius);
        const double height = radius * dial.window.height;
        const double width =
            height * (0.72 * static_cast<double>(dial.window.text.size()) + 0.8);
        svg << "<rect x='" << number(centre + at.x - width / 2.0) << "' y='"
            << number(centre + at.y - height * 0.72) << "' width='" << number(width)
            << "' height='" << number(height * 1.44) << "' rx='" << number(height * 0.18)
            << "' fill='" << background << "' stroke='" << bezel << "' stroke-width='"
            << number(size * 0.004) << "'/>";
        svg << "<text x='" << number(centre + at.x) << "' y='"
            << number(centre + at.y + height * 0.36)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(height) << "' font-weight='700' fill='" << cyan << "'>"
            << dial.window.text << "</text>";
    }

    // Lancette. Quando non ci sono, non ci sono: e' l'informazione principale
    // del quadrante guasto, e disegnarne una qualunque la cancellerebbe.
    //
    // Lunghezza e spessore distinguono le tre. Sull'altimetro a tre lancette la
    // lettura errata di mille piedi nasce dal confonderne due, e la differenza
    // di spessore e' cio' che la rende difficile.
    for (const Needle& needle : dial.needles) {
        double width = size * 0.016;
        const char* colour = primary;
        switch (needle.kind) {
            case NeedleKind::primary: width = size * 0.014; break;
            case NeedleKind::secondary: width = size * 0.026; break;
            case NeedleKind::tertiary: width = size * 0.034; break;
            // Indice fisso e indice del pilota: gialli, e distinti dalle
            // lancette perche' non sono misure.
            case NeedleKind::reference: width = size * 0.012; colour = amber; break;
            case NeedleKind::bug: width = size * 0.020; colour = cyan; break;
        }
        const Point tip = polar(needle.angle_deg, radius * needle.length);
        const Point tail = polar(needle.angle_deg + 180.0, radius * needle.tail);
        svg << "<line x1='" << number(centre + tail.x) << "' y1='" << number(centre + tail.y)
            << "' x2='" << number(centre + tip.x) << "' y2='" << number(centre + tip.y)
            << "' stroke='" << colour << "' stroke-width='" << number(width)
            << "' stroke-linecap='round'/>";
    }
    svg << "<circle cx='" << centre << "' cy='" << centre << "' r='" << number(size * 0.022)
        << "' fill='" << primary << "'/>";

    // Dicitura di stato. Ambra per cio' che si e' fermato, rosso per cio' che
    // non c'e' piu'.
    if (dial.annunciation != Annunciation::none) {
        const char* colour = dial.annunciation == Annunciation::failed ? warning : amber;
        svg << "<text x='" << centre << "' y='" << number(centre + radius * 0.72)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.15) << "' font-weight='700' fill='" << colour << "'>"
            << annunciation_text(dial.annunciation) << "</text>";
    }

    // Marcatura del dato simulato. Sul quadrante singolo e' una cornice, perche'
    // qui non esiste la pagina che la porterebbe: il pilota deve vederlo con la
    // visione periferica, senza doverlo leggere.
    if (dial.simulated) {
        const double inset = size * 0.02;
        svg << "<rect x='" << number(inset) << "' y='" << number(inset)
            << "' width='" << number(size - 2 * inset) << "' height='"
            << number(size - 2 * inset) << "' fill='none' stroke='" << amber
            << "' stroke-width='" << number(size * 0.012) << "' rx='" << number(size * 0.02)
            << "'/>";
        svg << "<text x='" << centre << "' y='" << number(size - inset * 2.2)
            << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif' font-size='"
            << number(radius * 0.12) << "' font-weight='700' fill='" << amber
            << "'>SIMULAZIONE</text>";
    }

    svg << "</svg>";
    return svg.str();
}

std::string render_svg(const Horizon& horizon, const DialStyle& style) {
    const double size = style.size_px;
    const double centre = size / 2.0;
    const double radius = size * 0.42;

    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << size << "' height='"
        << size << "' viewBox='0 0 " << size << " " << size << "'>";
    svg << "<rect width='" << size << "' height='" << size << "' fill='" << background
        << "'/>";
    svg << "<defs><clipPath id='face'><circle cx='" << centre << "' cy='" << centre
        << "' r='" << number(radius) << "'/></clipPath></defs>";

    if (horizon.has_attitude) {
        const double shift = radius * horizon.translation;
        // Il quadro ruota e trasla insieme, ritagliato dal vetro dello
        // strumento. Cielo e terra sono generosamente piu' grandi del vetro:
        // devono coprirlo a qualunque assetto.
        svg << "<g clip-path='url(#face)'><g transform='rotate("
            << number(horizon.rotation_deg) << " " << centre << " " << centre
            << ") translate(0 " << number(shift) << ")'>";
        svg << "<rect x='" << number(centre - radius * 3.0) << "' y='"
            << number(centre - radius * 3.0) << "' width='" << number(radius * 6.0)
            << "' height='" << number(radius * 3.0) << "' fill='" << sky << "'/>";
        svg << "<rect x='" << number(centre - radius * 3.0) << "' y='" << number(centre)
            << "' width='" << number(radius * 6.0) << "' height='" << number(radius * 3.0)
            << "' fill='" << earth << "'/>";
        svg << "<line x1='" << number(centre - radius * 3.0) << "' y1='" << number(centre)
            << "' x2='" << number(centre + radius * 3.0) << "' y2='" << number(centre)
            << "' stroke='" << primary << "' stroke-width='" << number(size * 0.008)
            << "'/>";

        for (const Rung& rung : horizon.ladder) {
            const double y = centre - radius * rung.offset;
            const double half = radius * rung.half_width;
            svg << "<line x1='" << number(centre - half) << "' y1='" << number(y)
                << "' x2='" << number(centre + half) << "' y2='" << number(y)
                << "' stroke='" << primary << "' stroke-width='" << number(size * 0.005)
                << "'/>";
            if (!rung.text.empty()) {
                const double text_height = radius * 0.09;
                for (const double side : {-1.0, 1.0}) {
                    svg << "<text x='" << number(centre + side * (half + text_height))
                        << "' y='" << number(y + text_height * 0.36)
                        << "' text-anchor='middle' font-family='DejaVu Sans, sans-serif'"
                        << " font-size='" << number(text_height) << "' fill='" << primary
                        << "'>" << rung.text << "</text>";
                }
            }
        }
        svg << "</g></g>";
    }

    // Ghiera e scala di inclinazione: fisse, perche' e' rispetto a loro che si
    // legge il rollio.
    svg << "<circle cx='" << centre << "' cy='" << centre << "' r='" << number(radius)
        << "' fill='none' stroke='" << bezel << "' stroke-width='" << number(size * 0.010)
        << "'/>";
    for (const Tick& tick : horizon.bank_scale) {
        const Point from = polar(tick.angle_deg, radius * tick.inner_radius);
        const Point to = polar(tick.angle_deg, radius * tick.outer_radius);
        svg << "<line x1='" << number(centre + from.x) << "' y1='" << number(centre + from.y)
            << "' x2='" << number(centre + to.x) << "' y2='" << number(centre + to.y)
            << "' stroke='" << primary << "' stroke-width='"
            << number(size * (tick.kind == TickKind::major ? 0.009 : 0.005))
            << "'/>";
    }

    if (horizon.has_attitude) {
        const Point pointer = polar(horizon.bank_pointer_deg, radius * 0.80);
        svg << "<circle cx='" << number(centre + pointer.x) << "' cy='"
            << number(centre + pointer.y) << "' r='" << number(size * 0.018)
            << "' fill='" << amber << "'/>";
    }

    // Sagoma dell'aeromobile, fissa al centro: e' il riferimento immobile
    // rispetto a cui il mondo si inclina.
    const double wing = radius * 0.38;
    svg << "<line x1='" << number(centre - wing) << "' y1='" << number(centre)
        << "' x2='" << number(centre - wing * 0.25) << "' y2='" << number(centre)
        << "' stroke='" << amber << "' stroke-width='" << number(size * 0.014) << "'/>";
    svg << "<line x1='" << number(centre + wing * 0.25) << "' y1='" << number(centre)
        << "' x2='" << number(centre + wing) << "' y2='" << number(centre)
        << "' stroke='" << amber << "' stroke-width='" << number(size * 0.014) << "'/>";
    svg << "<circle cx='" << centre << "' cy='" << centre << "' r='" << number(size * 0.012)
        << "' fill='" << amber << "'/>";

    svg << annunciation_markup(horizon.annunciation, horizon.simulated, size, centre, radius);
    svg << title_markup(style, size, centre, radius);
    svg << "</svg>";
    return svg.str();
}

std::string render_svg(const TurnCoordinator& coordinator, const DialStyle& style) {
    const double size = style.size_px;
    const double centre = size / 2.0;
    const double radius = size * 0.42;

    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << size << "' height='"
        << size << "' viewBox='0 0 " << size << " " << size << "'>";
    svg << "<rect width='" << size << "' height='" << size << "' fill='" << background
        << "'/>";
    svg << "<circle cx='" << centre << "' cy='" << centre << "' r='" << number(radius * 1.06)
        << "' fill='" << face << "' stroke='" << bezel << "' stroke-width='"
        << number(size * 0.006) << "'/>";

    // Tacche della virata standard: il riferimento per cui il quadrante esiste.
    for (const Tick& tick : coordinator.standard_marks) {
        const Point from = polar(tick.angle_deg - 90.0, radius * tick.inner_radius);
        const Point to = polar(tick.angle_deg - 90.0, radius * tick.outer_radius);
        svg << "<line x1='" << number(centre + from.x) << "' y1='" << number(centre + from.y)
            << "' x2='" << number(centre + to.x) << "' y2='" << number(centre + to.y)
            << "' stroke='" << primary << "' stroke-width='" << number(size * 0.010)
            << "'/>";
    }

    if (coordinator.has_turn) {
        const double wing = radius * 0.62;
        svg << "<g transform='rotate(" << number(coordinator.aircraft_bank_deg) << " "
            << centre << " " << number(centre - radius * 0.18) << ")'>";
        const double y = centre - radius * 0.18;
        svg << "<line x1='" << number(centre - wing) << "' y1='" << number(y) << "' x2='"
            << number(centre + wing) << "' y2='" << number(y) << "' stroke='" << primary
            << "' stroke-width='" << number(size * 0.020) << "' stroke-linecap='round'/>";
        svg << "<line x1='" << centre << "' y1='" << number(y - radius * 0.16) << "' x2='"
            << centre << "' y2='" << number(y + radius * 0.10) << "' stroke='" << primary
            << "' stroke-width='" << number(size * 0.016) << "' stroke-linecap='round'/>";
        svg << "</g>";
    }

    // Il tubo della pallina, con la pallina dentro. E' una grandezza diversa da
    // quella della sagoma, e occupa percio' una parte distinta del quadrante.
    const double tube_y = centre + radius * 0.55;
    const double tube_half = radius * 0.42;
    svg << "<rect x='" << number(centre - tube_half) << "' y='"
        << number(tube_y - radius * 0.11) << "' width='" << number(tube_half * 2.0)
        << "' height='" << number(radius * 0.22) << "' rx='" << number(radius * 0.11)
        << "' fill='" << background << "' stroke='" << bezel << "' stroke-width='"
        << number(size * 0.005) << "'/>";
    for (const double side : {-1.0, 1.0}) {
        const double x = centre + side * radius * 0.11;
        svg << "<line x1='" << number(x) << "' y1='" << number(tube_y - radius * 0.11)
            << "' x2='" << number(x) << "' y2='" << number(tube_y + radius * 0.11)
            << "' stroke='" << bezel << "' stroke-width='" << number(size * 0.005) << "'/>";
    }
    if (coordinator.has_ball) {
        svg << "<circle cx='"
            << number(centre + coordinator.ball_offset * (tube_half - radius * 0.10))
            << "' cy='" << number(tube_y) << "' r='" << number(radius * 0.09) << "' fill='"
            << primary << "'/>";
    }

    svg << annunciation_markup(coordinator.annunciation, coordinator.simulated, size,
                               centre, radius);
    svg << title_markup(style, size, centre, radius);
    svg << "</svg>";
    return svg.str();
}

}  // namespace efis::instruments
