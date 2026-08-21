// Prove della geometria degli strumenti analogici.
//
// Hanno un eseguibile proprio perche' verificano una categoria a se': non che
// un valore sia giusto, ma che il modo in cui viene mostrato non possa essere
// letto per quello che non e'. Una lancetta ferma su zero e una lancetta
// assente contengono la stessa quantita' di inchiostro e due informazioni
// opposte.

#include "efis/instruments/dial.hpp"
#include "efis/core/altimetry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close_to(double got, double expected, double tolerance = 1e-6) {
    return std::abs(got - expected) < tolerance;
}

std::size_t count_kind(const efis::instruments::Dial& dial,
                       efis::instruments::TickKind kind) {
    return static_cast<std::size_t>(
        std::count_if(dial.ticks.begin(), dial.ticks.end(),
                      [kind](const efis::instruments::Tick& tick) {
                          return tick.kind == kind;
                      }));
}

bool has_label(const efis::instruments::Dial& dial, const std::string& text) {
    return std::any_of(dial.labels.begin(), dial.labels.end(),
                       [&text](const efis::instruments::Label& label) {
                           return label.text == text;
                       });
}

efis::instruments::Reading valid(double value) {
    return {value, efis::Validity::valid, false};
}

}  // namespace

int main() {
    using namespace efis::instruments;

    // Anemometro di prova: la scala di un LSA, aperta in basso.
    const Scale asi{40.0, 160.0, -150.0, 300.0, 20.0, 5.0, 0, 0.0};
    const Sizing panel{480.0, 4.0, 28.0};

    // --- Convenzione degli angoli -------------------------------------------
    //
    // Zero a ore dodici e verso orario. Sbagliarla specchierebbe ogni
    // strumento, e uno strumento specchiato si legge benissimo: e' il tipo di
    // errore che non salta all'occhio.
    const Point twelve = polar(0.0, 1.0);
    require(close_to(twelve.x, 0.0) && close_to(twelve.y, -1.0),
            "zero degrees must point to twelve o'clock");
    const Point three = polar(90.0, 1.0);
    require(close_to(three.x, 1.0) && close_to(three.y, 0.0),
            "ninety degrees must point to three o'clock, so the sweep is clockwise");
    const Point nine = polar(270.0, 1.0);
    require(close_to(nine.x, -1.0) && close_to(nine.y, 0.0),
            "two hundred seventy degrees must point to nine o'clock");

    // --- Posizione della lancetta -------------------------------------------
    require(close_to(angle_for(asi, 40.0), -150.0),
            "the bottom of the scale must sit at the starting angle");
    require(close_to(angle_for(asi, 160.0), 150.0),
            "the top of the scale must sit at the end of the sweep");
    require(close_to(angle_for(asi, 100.0), 0.0),
            "the middle of a linear scale must sit at the middle of the sweep");

    // Il superamento del fondo scala non fa girare la lancetta.
    //
    // E' la regola piu' importante di tutte. Una lancetta lasciata proseguire
    // rientrerebbe dall'altra parte: a 200 nodi su una scala che finisce a 160
    // mostrerebbe una lettura bassa e credibile invece di una alta e
    // impossibile, e il pilota non avrebbe modo di accorgersene.
    require(close_to(angle_for(asi, 500.0), angle_for(asi, 160.0)),
            "a value beyond the top must stop at the top, never wrap around");
    require(close_to(angle_for(asi, -100.0), angle_for(asi, 40.0)),
            "a value below the bottom must stop at the bottom");

    const Dial over = describe(asi, valid(200.0), panel);
    require(over.annunciation == Annunciation::out_of_range,
            "a reading beyond the scale must be declared, not just clamped");
    require(over.needles.size() == 1,
            "an out of range reading still shows where the value has gone");
    require(std::string{annunciation_text(over.annunciation)} == "RANGE",
            "the out of range wording must stay stable");

    // Senza la dicitura, una lancetta a fondo scala sarebbe indistinguibile da
    // una lettura esattamente pari al fondo scala. Con la dicitura, no.
    const Dial at_top = describe(asi, valid(160.0), panel);
    require(at_top.annunciation == Annunciation::none,
            "a reading exactly at the top is a valid reading, not an overrun");
    require(close_to(at_top.needles.front().angle_deg, over.needles.front().angle_deg),
            "the two needles sit in the same place, which is why the wording matters");

    // --- Dato assente: nessuna lancetta -------------------------------------
    //
    // Uno strumento guasto che mostrasse la lancetta su zero verrebbe letto
    // come una misura, perche' zero e' una lettura possibile. La lancetta
    // sparisce e resta la bandierina.
    const Dial failed = describe(asi, {0.0, efis::Validity::unavailable, false}, panel);
    require(failed.needles.empty(),
            "a failed instrument must not show a needle parked on zero");
    require(failed.annunciation == Annunciation::failed,
            "a failed instrument must say so");
    require(!failed.ticks.empty(),
            "the dial face stays: it is the reading that is missing, not the scale");

    const Dial stale = describe(asi, {95.0, efis::Validity::stale, false}, panel);
    require(stale.needles.empty(),
            "a stale reading must not keep pointing as if it were current");
    require(stale.annunciation == Annunciation::stale,
            "a stale reading must be told apart from a missing one");

    // Il dato simulato resta dichiarato: su una pagina mista non tutti i
    // quadranti hanno la stessa provenienza.
    const Dial simulated = describe(asi, {95.0, efis::Validity::simulated, true}, panel);
    require(simulated.simulated, "a simulated reading must be marked as such");
    require(simulated.needles.size() == 1,
            "simulated data is still usable and still gets a needle");

    // --- Fasce dal profilo ---------------------------------------------------
    //
    // Gli archi non sono decorazione: dicono dove si puo' volare. Vengono dalle
    // velocita' del profilo aeromobile, non da numeri scritti nel codice.
    const std::vector<BandRange> asi_bands{
        {39.0, 85.0, Band::white},    // gamma con ipersostentatori estesi
        {45.0, 120.0, Band::green},   // gamma di funzionamento normale
        {120.0, 140.0, Band::yellow}, // gamma da usare con cautela
        {140.0, 200.0, Band::red}     // oltre la velocita' da non superare
    };
    const Dial with_bands = describe(asi, valid(95.0), panel, asi_bands);
    require(with_bands.arcs.size() == 4, "every band within the scale must be drawn");

    // Le fasce che sbordano vengono ritagliate sui limiti della scala, non
    // disegnate fuori dal quadrante.
    require(close_to(with_bands.arcs.front().start_deg, angle_for(asi, 40.0)),
            "a band starting below the scale must be trimmed to the scale");
    require(close_to(with_bands.arcs.back().end_deg, angle_for(asi, 160.0)),
            "a band running past the top must be trimmed to the top");

    // Una fascia interamente fuori scala non si disegna. Schiacciata contro un
    // estremo indicherebbe un limite che li' non c'e'.
    const Dial outside = describe(asi, valid(95.0), panel, {{10.0, 30.0, Band::red}});
    require(outside.arcs.empty(),
            "a band entirely outside the scale must not be drawn at all");

    // --- Leggibilita' al variare della dimensione ----------------------------
    //
    // La stessa scala su un pannello e su un display circolare piccolo. Sotto
    // una certa distanza due tacche non si distinguono piu', e disegnarle rende
    // il quadrante peggiore invece che piu' preciso.
    const Sizing round_small{240.0, 4.0, 28.0};
    const Scale fine{0.0, 3000.0, -150.0, 300.0, 500.0, 50.0, 0, 0.0};

    const Dial big = describe(fine, valid(2380.0), panel);
    const Dial small = describe(fine, valid(2380.0), round_small);
    require(count_kind(small, TickKind::minor) <= count_kind(big, TickKind::minor),
            "a smaller dial must never carry more minor ticks than a larger one");
    require(small.labels.size() <= big.labels.size(),
            "a smaller dial must never carry more labels than a larger one");
    require(!small.labels.empty(), "even the smallest dial keeps some labels");
    require(!small.needles.empty(), "thinning the face must not remove the reading");

    // Le tacche che restano cadono ancora sui valori tondi: si diradano
    // raddoppiando il passo, quindi sono un sottoinsieme di quelle originali.
    require(has_label(big, "0") && has_label(big, "3000"),
            "the ends of the scale must be labelled");
    require(has_label(small, "0"),
            "the start of the scale survives the thinning");

    // Una dimensione assurda non deve produrre un quadrante assurdo.
    const Dial tiny = describe(fine, valid(1000.0), Sizing{48.0, 4.0, 28.0});
    require(tiny.needles.size() == 1, "even a tiny dial shows its reading");
    require(tiny.ticks.size() < big.ticks.size(),
            "a tiny dial must be far less dense than a panel one");

    // --- Tacche lunghe e corte non si sovrappongono --------------------------
    //
    // Dove cade una tacca lunga non ne va disegnata anche una corta: si
    // vedrebbe una tacca sola, piu' spessa, in posizioni scelte a caso.
    const Dial overlap = describe(Scale{0.0, 100.0, -150.0, 300.0, 20.0, 10.0, 0, 0.0},
                                  valid(50.0), panel);
    require(count_kind(overlap, TickKind::major) == 6,
            "a scale from zero to one hundred by twenty must have six long ticks");
    require(count_kind(overlap, TickKind::minor) == 5,
            "the short ticks falling on a long one must not be drawn twice");

    // --- Scala che si avvolge ------------------------------------------------
    //
    // La lancetta delle centinaia dell'altimetro compie un giro ogni mille
    // piedi e non ha estremi da cui uscire: per lei il limite non ha senso, e
    // infatti non viene applicato.
    const Scale hundreds{0.0, 1000.0, 0.0, 360.0, 100.0, 20.0, 0, 1000.0};
    require(close_to(angle_for(hundreds, 0.0), 0.0),
            "the wrapping needle starts at the top");
    require(close_to(angle_for(hundreds, 500.0), 180.0),
            "half a turn is half the wrap");
    require(close_to(angle_for(hundreds, 1000.0), 0.0),
            "one full wrap returns to the start");
    require(close_to(angle_for(hundreds, 2500.0), angle_for(hundreds, 500.0)),
            "the wrapping needle only shows the remainder, as on a real altimeter");
    require(close_to(angle_for(hundreds, -250.0), angle_for(hundreds, 750.0)),
            "a negative value must wrap forwards, not produce a negative angle");

    const Dial wrapped = describe(hundreds, valid(5400.0), panel);
    require(wrapped.annunciation == Annunciation::none,
            "a wrapping scale has no ends, so it cannot be out of range");

    // --- Scala malformata ----------------------------------------------------
    //
    // Una scala senza ampiezza non produce un quadrante vuoto che sembra
    // funzionante: si dichiara guasta.
    const Dial degenerate = describe(Scale{50.0, 50.0, -150.0, 300.0, 10.0, 2.0, 0, 0.0},
                                     valid(50.0), panel);
    require(degenerate.annunciation == Annunciation::failed,
            "a scale with no span must declare itself failed");
    require(degenerate.needles.empty(),
            "a malformed scale must not show a needle either");

    // --- Altimetria: regolazione barometrica e quota indicata ----------------
    //
    // Le due quote restano distinte per una ragione pratica: registrare solo la
    // quota indicata renderebbe incomparabili due voli condotti con regolazioni
    // diverse, e registrare solo quella di pressione toglierebbe la quota che il
    // pilota stava effettivamente leggendo.
    require(close_to(efis::indicated_altitude_ft(5000.0, efis::standard_pressure_hpa),
                     5000.0),
            "with the standard setting the indicated altitude is the pressure altitude");
    require(efis::indicated_altitude_ft(5000.0, efis::standard_pressure_hpa) == 5000.0,
            "the standard setting must return the value exactly, not approximately");

    // Regolazione piu' alta della standard: l'altimetro indica di piu'. E' il
    // verso giusto, e sbagliarlo darebbe un errore che cresce proprio quando la
    // pressione si allontana dalla standard.
    const double high = efis::indicated_altitude_ft(5000.0, 1030.0);
    const double low = efis::indicated_altitude_ft(5000.0, 1000.0);
    require(high > 5000.0, "a setting above standard must indicate a higher altitude");
    require(low < 5000.0, "a setting below standard must indicate a lower altitude");

    // Vicino al livello del mare la regola pratica dei ventisette piedi per
    // ettopascal deve essere rispettata a meno di poco: se non lo fosse,
    // sarebbe la formula a essere sbagliata, non la regola.
    const double per_hpa = efis::indicated_altitude_ft(0.0, efis::standard_pressure_hpa + 1.0);
    require(per_hpa > 26.0 && per_hpa < 29.0,
            "near sea level one hectopascal must be worth about twenty seven feet");

    // Andata e ritorno: la conversione inversa deve riportare al punto di
    // partenza. E' cio' che garantisce che le due formule descrivano la stessa
    // atmosfera.
    const double pressure_at_8000 = efis::pressure_from_altitude_hpa(8000.0);
    require(close_to(efis::altitude_from_pressure_ft(pressure_at_8000,
                                                     efis::standard_pressure_hpa),
                     8000.0, 1e-4),
            "converting altitude to pressure and back must return the same altitude");
    require(pressure_at_8000 < efis::standard_pressure_hpa,
            "pressure must fall with altitude");

    // Conversione fra le due unita' della finestra di Kollsman.
    require(close_to(efis::inches_to_hectopascal(29.92), 1013.2, 0.1),
            "29.92 inches of mercury is the standard setting in hectopascals");
    require(close_to(efis::hectopascal_to_inches(efis::inches_to_hectopascal(30.12)), 30.12),
            "the unit conversion must be reversible");

    // Una regolazione implausibile non e' una pressione: e' un errore di
    // inserimento.
    require(!efis::is_plausible_setting(0.0), "zero is not an altimeter setting");
    require(!efis::is_plausible_setting(2992.0),
            "inches typed into a hectopascal field must be rejected, not applied");
    require(efis::is_plausible_setting(1013.25), "the standard setting is plausible");
    require(efis::is_plausible_setting(950.0), "a deep depression is still plausible");


    std::cout << "All EFIS instrument tests passed\n";
}
