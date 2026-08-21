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

Dial describe(const Scale& scale, const Reading& reading, const Sizing& sizing,
              const std::vector<BandRange>& bands) {
    Dial dial;
    dial.simulated = reading.simulated;

    const double span = scale.maximum - scale.minimum;
    if (span <= 0.0) {
        dial.annunciation = Annunciation::failed;
        return dial;
    }

    // --- Fasce ---------------------------------------------------------------
    //
    // Vanno per prime perche' stanno sotto tutto il resto. Una fascia che
    // sborda viene ritagliata; una che cade interamente fuori non si disegna,
    // invece di comparire schiacciata contro un estremo dove non significa piu'
    // nulla.
    for (const BandRange& range : bands) {
        const double from = std::max(range.from, scale.minimum);
        const double to = std::min(range.to, scale.maximum);
        if (from >= to) continue;
        dial.arcs.push_back(Arc{angle_for(scale, from), angle_for(scale, to),
                                band_inner, band_outer, range.band});
    }

    // --- Tacche e scritte ----------------------------------------------------
    const double major_factor =
        thinning(spacing_px(scale, scale.major_step, sizing), sizing.minimum_tick_spacing_px);
    const double minor_factor =
        thinning(spacing_px(scale, scale.minor_step, sizing), sizing.minimum_tick_spacing_px);

    const double major_step = scale.major_step * major_factor;
    const double minor_step = scale.minor_step * minor_factor;

    // Le scritte si diradano per conto loro: stanno su una tacca lunga, ma
    // occupano molto piu' spazio di una tacca.
    const double label_factor =
        thinning(spacing_px(scale, major_step, sizing), sizing.minimum_label_spacing_px);
    const double label_step = major_step * label_factor;

    if (minor_step > 0.0) {
        // La tolleranza serve a non perdere l'ultima tacca per un errore di
        // arrotondamento sull'ultimo passo.
        const double tolerance = minor_step * 1e-6;
        for (double value = scale.minimum; value <= scale.maximum + tolerance;
             value += minor_step) {
            // Le posizioni occupate da una tacca lunga non ricevono anche
            // quella corta.
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
        for (double value = scale.minimum; value <= scale.maximum + tolerance;
             value += major_step) {
            const double angle = angle_for(scale, value);
            dial.ticks.push_back(Tick{angle, major_tick_inner, tick_outer, TickKind::major});

            const double from_start = value - scale.minimum;
            const double remainder = std::fmod(from_start, label_step);
            const bool labelled = label_step > 0.0 &&
                (remainder < tolerance || label_step - remainder < tolerance);
            if (labelled) {
                dial.labels.push_back(Label{angle, label_radius, label_height,
                                            format(value, scale.decimals)});
            }
        }
    }

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

}  // namespace efis::instruments
