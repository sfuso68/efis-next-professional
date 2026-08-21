#include "efis/instruments/dial.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace efis::instruments {
namespace {

constexpr double pi = 3.14159265358979323846;

// Raggi delle parti del quadrante, in frazione del raggio totale. Sono qui e
// non sparsi nel codice perche' definiscono l'aspetto comune a tutti gli
// strumenti: cambiarli qui li cambia tutti insieme.
constexpr double major_tick_inner = 0.80;
constexpr double minor_tick_inner = 0.88;
constexpr double tick_outer = 0.97;
constexpr double label_radius = 0.68;
constexpr double label_height = 0.13;
constexpr double band_inner = 0.90;
constexpr double band_outer = 1.00;
constexpr double needle_length = 0.82;
constexpr double needle_tail = 0.12;

std::string format(double value, int decimals) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(std::max(0, decimals)) << value;
    return out.str();
}

// Distanza in pixel fra due valori adiacenti sulla scala, alla dimensione data.
double spacing_px(const Scale& scale, double step, const Sizing& sizing) {
    const double span = scale.maximum - scale.minimum;
    if (span <= 0.0 || step <= 0.0) return 0.0;
    const double arc_length = pi * sizing.diameter_px * (std::abs(scale.sweep_deg) / 360.0);
    return arc_length * (step / span);
}

// Moltiplicatore da applicare a un passo perche' le tacche restino separate di
// almeno `minimum`. Raddoppia finche' non basta, cosi' le tacche che restano
// sono un sottoinsieme di quelle originali e cadono ancora sui valori tondi.
double thinning(double spacing, double minimum) {
    double factor = 1.0;
    while (spacing > 0.0 && spacing * factor < minimum && factor < 1024.0) {
        factor *= 2.0;
    }
    return factor;
}

}  // namespace

Point polar(double angle_deg, double radius) noexcept {
    // Zero a ore dodici e verso orario: si ruota di novanta gradi il sistema
    // trigonometrico e se ne inverte il verso.
    const double radians = (angle_deg - 90.0) * pi / 180.0;
    return Point{radius * std::cos(radians), radius * std::sin(radians)};
}

double angle_for(const Scale& scale, double value) noexcept {
    const double span = scale.maximum - scale.minimum;
    if (span <= 0.0) return scale.start_deg;

    if (scale.wrap > 0.0) {
        // Scala che si avvolge: la lancetta compie un giro ogni `wrap` unita' e
        // non ha estremi da cui uscire.
        double position = std::fmod(value, scale.wrap);
        if (position < 0.0) position += scale.wrap;
        return scale.start_deg + scale.sweep_deg * (position / scale.wrap);
    }

    // Limitare agli estremi non e' una comodita': una lancetta lasciata
    // proseguire oltre il fondo scala rientrerebbe dall'altra parte e
    // mostrerebbe una lettura bassa credibile al posto di una alta impossibile.
    const double clamped = std::clamp(value, scale.minimum, scale.maximum);
    return scale.start_deg + scale.sweep_deg * ((clamped - scale.minimum) / span);
}

namespace {

// Costruisce fasce, tacche e scritte. E' condivisa fra il quadrante a scala
// lineare e l'altimetro, che hanno lancette diverse ma la stessa faccia.
//
// `label_scale` moltiplica il valore prima di scriverlo: la faccia
// dell'altimetro e' graduata in centinaia di piedi e riporta le cifre da 0 a 9,
// non da 0 a 900.
void build_face(const Scale& scale, const Sizing& sizing,
                const std::vector<BandRange>& bands, double label_scale, Dial& dial) {
    for (const BandRange& range : bands) {
        const double from = std::max(range.from, scale.minimum);
        const double to = std::min(range.to, scale.maximum);
        if (from >= to) continue;
        dial.arcs.push_back(Arc{angle_for(scale, from), angle_for(scale, to),
                                band_inner, band_outer, range.band});
    }

    const double major_factor =
        thinning(spacing_px(scale, scale.major_step, sizing), sizing.minimum_tick_spacing_px);
    const double minor_factor =
        thinning(spacing_px(scale, scale.minor_step, sizing), sizing.minimum_tick_spacing_px);

    const double major_step = scale.major_step * major_factor;
    const double minor_step = scale.minor_step * minor_factor;

    const double label_factor =
        thinning(spacing_px(scale, major_step, sizing), sizing.minimum_label_spacing_px);
    const double label_step = major_step * label_factor;

    // Su una faccia che si chiude su se stessa l'ultima tacca coincide con la
    // prima: disegnarle entrambe lascerebbe due tacche sovrapposte a ore dodici.
    const bool closed = scale.wrap > 0.0 && std::abs(scale.sweep_deg) >= 360.0;
    const double limit = closed ? scale.maximum - minor_step / 2.0 : scale.maximum;

    if (minor_step > 0.0) {
        const double tolerance = minor_step * 1e-6;
        for (double value = scale.minimum; value <= limit + tolerance; value += minor_step) {
            const double from_start = value - scale.minimum;
            const double remainder = std::fmod(from_start, major_step);
            const bool on_major = major_step > 0.0 &&
                (remainder < tolerance || major_step - remainder < tolerance);
            if (on_major) continue;
            dial.ticks.push_back(Tick{angle_for(scale, value), minor_tick_inner,
                                      tick_outer, TickKind::minor});
        }
    }

    if (major_step > 0.0) {
        const double tolerance = major_step * 1e-6;
        const double major_limit = closed ? scale.maximum - major_step / 2.0 : scale.maximum;
        for (double value = scale.minimum; value <= major_limit + tolerance;
             value += major_step) {
            const double angle = angle_for(scale, value);
            dial.ticks.push_back(Tick{angle, major_tick_inner, tick_outer, TickKind::major});

            const double from_start = value - scale.minimum;
            const double remainder = std::fmod(from_start, label_step);
            const bool labelled = label_step > 0.0 &&
                (remainder < tolerance || label_step - remainder < tolerance);
            if (labelled) {
                dial.labels.push_back(Label{angle, label_radius, label_height,
                                            format(value * label_scale, scale.decimals)});
            }
        }
    }
}

}  // namespace

Dial describe(const Scale& scale, const Reading& reading, const Sizing& sizing,
              const std::vector<BandRange>& bands) {
    Dial dial;
    dial.simulated = reading.simulated;

    const double span = scale.maximum - scale.minimum;
    if (span <= 0.0) {
        dial.annunciation = Annunciation::failed;
        return dial;
    }

    build_face(scale, sizing, bands, 1.0, dial);

    // --- Lancetta e annuncio -------------------------------------------------
    //
    // L'ordine conta: prima si decide se c'e' qualcosa da mostrare, poi lo si
    // mostra. Uno strumento senza dato non riceve lancetta.
    if (!is_usable(reading.validity)) {
        dial.annunciation = reading.validity == Validity::stale
            ? Annunciation::stale
            : Annunciation::failed;
        return dial;
    }

    if (scale.wrap <= 0.0 &&
        (reading.value < scale.minimum || reading.value > scale.maximum)) {
        // La lancetta resta all'estremo e lo strumento dichiara il superamento.
        // Senza la dicitura una lancetta ferma a fondo scala sarebbe
        // indistinguibile da una lettura esattamente pari al fondo scala.
        dial.annunciation = Annunciation::out_of_range;
    }

    dial.needles.push_back(Needle{angle_for(scale, reading.value), needle_length,
                                  needle_tail, NeedleKind::primary});
    return dial;
}

// Limiti di plausibilita' della quota.
//
// Le lancette dell'altimetro si avvolgono e non hanno estremi da cui uscire: e'
// proprio questo a renderlo pericoloso di fronte a un dato corrotto. Un valore
// di un milione di piedi si avvolgerebbe e mostrerebbe una quota bassa
// perfettamente credibile. Fuori da questo intervallo lo strumento non indica
// nulla e lo dichiara.
namespace {
constexpr double lowest_altitude_ft = -1500.0;
constexpr double highest_altitude_ft = 50000.0;
}  // namespace

Dial describe_altimeter(const AltimeterReading& reading, const Sizing& sizing) {
    // La faccia e' graduata in centinaia di piedi e riporta le cifre da 0 a 9.
    const Scale face{0.0, 1000.0, 0.0, 360.0, 100.0, 20.0, 0, 1000.0};

    Dial dial;
    dial.simulated = reading.altitude.simulated;
    build_face(face, sizing, {}, 0.01, dial);

    // La finestrella e' a ore tre, dove sta su quasi tutti gli altimetri. Vuota
    // significa regolazione non disponibile: mostrare 1013 direbbe che e'
    // standard, che e' un'affermazione diversa.
    dial.window = Window{!reading.setting_text.empty(), 90.0, 0.55, 0.13,
                         reading.setting_text};

    if (!is_usable(reading.altitude.validity)) {
        dial.annunciation = reading.altitude.validity == Validity::stale
            ? Annunciation::stale
            : Annunciation::failed;
        return dial;
    }

    const double altitude = reading.altitude.value;
    if (altitude < lowest_altitude_ft || altitude > highest_altitude_ft) {
        // Nessuna lancetta: avvolta, indicherebbe una quota credibile e falsa.
        dial.annunciation = Annunciation::out_of_range;
        return dial;
    }

    // Tre decadi, tre lancette. Ciascuna compie un giro quando la precedente ne
    // compie un decimo, e la piu' lunga e' quella che si muove piu' in fretta.
    const Scale hundreds{0.0, 1000.0, 0.0, 360.0, 100.0, 20.0, 0, 1000.0};
    const Scale thousands{0.0, 10000.0, 0.0, 360.0, 1000.0, 200.0, 0, 10000.0};
    const Scale ten_thousands{0.0, 100000.0, 0.0, 360.0, 10000.0, 2000.0, 0, 100000.0};

    dial.needles.push_back(Needle{angle_for(ten_thousands, altitude), 0.34, 0.10,
                                  NeedleKind::tertiary});
    dial.needles.push_back(Needle{angle_for(thousands, altitude), 0.56, 0.12,
                                  NeedleKind::secondary});
    dial.needles.push_back(Needle{angle_for(hundreds, altitude), 0.84, 0.14,
                                  NeedleKind::primary});
    return dial;
}

}  // namespace efis::instruments
