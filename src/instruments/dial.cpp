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

// Lunghezza della circonferenza alla dimensione data. Serve a decidere se due
// tacche restano distinguibili, come per i quadranti a scala.
double pi_arc_length(double diameter_px) {
    return pi * diameter_px;
}

// Le scritte della rosa sono i punti cardinali e le decine di gradi: e' la
// convenzione di ogni bussola, e scrivere 090 al posto di E la renderebbe
// piu' lenta da leggere proprio nei momenti in cui serve una lettura rapida.
std::string cardinal_text(double bearing_deg) {
    const int rounded = static_cast<int>(bearing_deg + 0.5) % 360;
    switch (rounded) {
        case 0: return "N";
        case 90: return "E";
        case 180: return "S";
        case 270: return "W";
        default: break;
    }
    return std::to_string(rounded / 10);
}

const std::vector<BandRange>& empty_bands() {
    static const std::vector<BandRange> none;
    return none;
}

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
void build_face(const Scale& scale, const Sizing& sizing, const Limits& limits,
                double label_scale, Dial& dial) {
    dial.limits = limits.provenance;

    // Una provenienza assente non disegna nulla, quali che siano le fasce
    // passate. Un quadrante senza archi dichiara di non sapere dove siano i
    // limiti; uno con archi che nessuno ha verificato afferma di saperlo.
    const std::vector<BandRange>& bands =
        limits.provenance == LimitsProvenance::absent ? empty_bands() : limits.bands;

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
              const Limits& limits) {
    Dial dial;
    dial.simulated = reading.simulated;

    const double span = scale.maximum - scale.minimum;
    if (span <= 0.0) {
        dial.annunciation = Annunciation::failed;
        return dial;
    }

    build_face(scale, sizing, limits, 1.0, dial);

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
    build_face(face, sizing, Limits{}, 0.01, dial);

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

double normalise_heading(double heading_deg) noexcept {
    double value = std::fmod(heading_deg, 360.0);
    if (value < 0.0) value += 360.0;
    return value;
}

Dial describe_heading(const HeadingReading& reading, const Sizing& sizing) {
    Dial dial;
    dial.simulated = reading.heading.simulated;

    // La linea di fede e' fissa e appartiene al quadrante, non alla misura:
    // resta anche quando la prua manca, perche' indica dove si leggerebbe.
    dial.needles.push_back(Needle{0.0, 1.02, 0.0, NeedleKind::reference});

    if (!is_usable(reading.heading.validity)) {
        // Nessuna rosa. Disegnarla ferma sarebbe la riproduzione fedele di un
        // girodirezionale guasto, che e' esattamente il guasto da non imitare:
        // una rosa ferma si legge come una prua.
        dial.annunciation = reading.heading.validity == Validity::stale
            ? Annunciation::stale
            : Annunciation::failed;
        return dial;
    }

    const double heading = normalise_heading(reading.heading.value);

    // Tacche ogni cinque gradi, lunghe ogni dieci, scritte ogni trenta. E' la
    // graduazione di ogni bussola: le scritte sono i punti cardinali e le
    // decine di gradi.
    const double tick_step =
        thinning(pi_arc_length(sizing.diameter_px) * (5.0 / 360.0),
                 sizing.minimum_tick_spacing_px) * 5.0;

    for (double bearing = 0.0; bearing < 360.0; bearing += tick_step) {
        const double angle = normalise_heading(bearing - heading);
        const bool major = std::fmod(bearing, 10.0) < 1e-6;
        dial.ticks.push_back(Tick{angle,
                                  major ? major_tick_inner : minor_tick_inner,
                                  tick_outer,
                                  major ? TickKind::major : TickKind::minor});
    }

    const double label_step =
        thinning(pi_arc_length(sizing.diameter_px) * (30.0 / 360.0),
                 sizing.minimum_label_spacing_px) * 30.0;

    for (double bearing = 0.0; bearing < 360.0; bearing += label_step) {
        const double angle = normalise_heading(bearing - heading);
        dial.labels.push_back(Label{angle, label_radius, label_height * 1.1,
                                    cardinal_text(bearing)});
    }

    if (is_usable(reading.bug.validity)) {
        dial.needles.push_back(
            Needle{normalise_heading(reading.bug.value - heading), 1.0, 0.0,
                   NeedleKind::bug});
    }

    return dial;
}

Dial describe_cluster(const Scale& scale, const ClusterReading& reading,
                      const Sizing& sizing, const Limits& limits) {
    Dial dial;
    dial.simulated = reading.simulated;

    const double span = scale.maximum - scale.minimum;
    if (span <= 0.0) {
        dial.annunciation = Annunciation::failed;
        dial.limits = limits.provenance;
        return dial;
    }

    build_face(scale, sizing, limits, 1.0, dial);

    std::size_t shown = 0;
    bool any_out_of_range = false;
    for (const Reading& channel : reading.channels) {
        if (!is_usable(channel.validity)) continue;
        ++shown;
        if (channel.value < scale.minimum || channel.value > scale.maximum) {
            any_out_of_range = true;
        }
        // Tutte le lancette hanno lo stesso peso: i canali sono omogenei, e
        // dare a uno la lancetta principale suggerirebbe una gerarchia che non
        // esiste fra quattro cilindri.
        dial.needles.push_back(Needle{angle_for(scale, channel.value), needle_length,
                                      needle_tail, NeedleKind::primary});
    }

    if (shown == 0) {
        // Nessun canale: lo strumento non ha nulla da mostrare. Distinguere
        // scaduto da assente richiede che almeno un canale lo dichiari.
        const bool stale =
            std::any_of(reading.channels.begin(), reading.channels.end(),
                        [](const Reading& channel) {
                            return channel.validity == Validity::stale;
                        });
        dial.annunciation = stale ? Annunciation::stale : Annunciation::failed;
        return dial;
    }

    if (shown < reading.channels.size()) {
        // Parziale ha la precedenza sul fuori scala: sapere che manca un
        // cilindro conta piu' che sapere che un altro e' alto, perche' il
        // confronto fra loro e' l'informazione per cui il quadrante esiste.
        dial.annunciation = Annunciation::partial;
    } else if (any_out_of_range) {
        dial.annunciation = Annunciation::out_of_range;
    }

    return dial;
}

}  // namespace efis::instruments
