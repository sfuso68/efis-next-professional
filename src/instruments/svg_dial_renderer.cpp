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
        switch (needle.kind) {
            case NeedleKind::primary: width = size * 0.014; break;
            case NeedleKind::secondary: width = size * 0.026; break;
            case NeedleKind::tertiary: width = size * 0.034; break;
        }
        const Point tip = polar(needle.angle_deg, radius * needle.length);
        const Point tail = polar(needle.angle_deg + 180.0, radius * needle.tail);
        svg << "<line x1='" << number(centre + tail.x) << "' y1='" << number(centre + tail.y)
            << "' x2='" << number(centre + tip.x) << "' y2='" << number(centre + tip.y)
            << "' stroke='" << primary << "' stroke-width='" << number(width)
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

}  // namespace efis::instruments
