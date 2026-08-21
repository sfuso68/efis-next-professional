// Prove della geometria degli strumenti analogici.
//
// Hanno un eseguibile proprio perche' verificano una categoria a se': non che
// un valore sia giusto, ma che il modo in cui viene mostrato non possa essere
// letto per quello che non e'. Una lancetta ferma su zero e una lancetta
// assente contengono la stessa quantita' di inchiostro e due informazioni
// opposte.

#include "efis/instruments/dial.hpp"
#include "efis/instruments/horizon.hpp"
#include "efis/instruments/panel.hpp"
#include "efis/instruments/panel_store.hpp"
#include "efis/core/altimetry.hpp"
#include "efis/core/flight_dynamics.hpp"
#include "efis/providers/simulation_provider.hpp"
#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/core/signal_access.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    const Dial with_bands = describe(asi, valid(95.0), panel, Limits{asi_bands, LimitsProvenance::indicative});
    require(with_bands.arcs.size() == 4, "every band within the scale must be drawn");

    // Le fasce che sbordano vengono ritagliate sui limiti della scala, non
    // disegnate fuori dal quadrante.
    require(close_to(with_bands.arcs.front().start_deg, angle_for(asi, 40.0)),
            "a band starting below the scale must be trimmed to the scale");
    require(close_to(with_bands.arcs.back().end_deg, angle_for(asi, 160.0)),
            "a band running past the top must be trimmed to the top");

    // Una fascia interamente fuori scala non si disegna. Schiacciata contro un
    // estremo indicherebbe un limite che li' non c'e'.
    const Dial outside = describe(asi, valid(95.0), panel, Limits{{{10.0, 30.0, Band::red}}, LimitsProvenance::indicative});
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


    // --- Virata corretta: la fisica che alimenta il virosbandometro ----------
    //
    // La relazione sta nel core e non nel simulatore perche' non serve solo a
    // generare dati finti: servira' al confronto incrociato per verificare che
    // il rateo misurato dal Pixhawk sia compatibile con inclinazione e
    // velocita'. Un giroscopio bloccato continua a dichiarare un rateo, e il
    // modo per accorgersene e' sapere quale rateo dovrebbe avere.
    require(close_to(efis::coordinated_turn_rate_deg_s(0.0, 100.0), 0.0),
            "wings level means no turn, whatever the speed");
    require(efis::coordinated_turn_rate_deg_s(30.0, 100.0) > 0.0,
            "banking right must turn right");
    require(close_to(efis::coordinated_turn_rate_deg_s(-30.0, 100.0),
                     -efis::coordinated_turn_rate_deg_s(30.0, 100.0)),
            "the turn must be symmetric about wings level");

    // A parita' di inclinazione, piu' si va veloce piu' la virata e' larga.
    require(efis::coordinated_turn_rate_deg_s(30.0, 60.0) >
                efis::coordinated_turn_rate_deg_s(30.0, 120.0),
            "at the same bank a faster aircraft turns more slowly");

    // Un aeromobile fermo e inclinato non sta virando: sta cadendo. Senza
    // questo caso il quadrante mostrerebbe fondo scala per un aeroplano
    // parcheggiato.
    require(close_to(efis::coordinated_turn_rate_deg_s(45.0, 0.0), 0.0),
            "a standing aircraft is not turning, however it is banked");

    // La virata di riferimento della navigazione strumentale: tre gradi al
    // secondo, un giro in due minuti. La regola pratica dice che a 100 nodi
    // serve circa un'inclinazione di 17 gradi, e la formula deve confermarlo.
    const double standard_bank = efis::bank_for_turn_rate_deg(efis::standard_rate_deg_s, 100.0);
    require(standard_bank > 14.0 && standard_bank < 20.0,
            "a standard rate turn at one hundred knots needs about seventeen degrees");

    // Andata e ritorno fra le due relazioni.
    const double recovered = efis::coordinated_turn_rate_deg_s(standard_bank, 100.0);
    require(close_to(recovered, efis::standard_rate_deg_s, 1e-9),
            "bank for a rate and rate for that bank must agree");

    // Il simulatore pubblica le due grandezze, e sono coerenti con la propria
    // inclinazione: e' cio' che permette di collegare il virosbandometro al
    // simulatore oggi e al Pixhawk domani senza toccare l'indicatore.
    efis::SimulationProvider turning{11};
    const auto flying = turning.sample(efis::MonoTime{} + std::chrono::seconds{30},
                                       std::chrono::seconds{30});
    require(flying.attitude.turn_rate_deg_s.origin.signal == efis::SignalId::turn_rate,
            "the simulator must declare which signal the turn rate is");
    require(flying.attitude.turn_rate_deg_s.origin.unit == efis::Unit::degree_per_second,
            "the turn rate must declare its unit");
    require(flying.attitude.lateral_acceleration_g.origin.unit == efis::Unit::gravity,
            "lateral acceleration is measured in multiples of gravity");
    require(close_to(flying.attitude.turn_rate_deg_s.value,
                     efis::coordinated_turn_rate_deg_s(
                         flying.attitude.roll_deg.value,
                         flying.air_data.indicated_airspeed_kt.value),
                     1e-9),
            "the simulated turn rate must agree with its own bank and speed");
    require(flying.attitude.turn_rate_deg_s.origin.simulated,
            "simulated dynamics stay marked as simulated");


    // --- Accesso alle grandezze per identificativo ---------------------------
    //
    // Il renderer espone una proprieta' per grandezza. Finche' gli strumenti li
    // decide chi scrive il codice quel modello regge; con strumenti scelti dal
    // pilota no, perche' il pannello deve poter chiedere una grandezza che
    // nessuno aveva previsto sarebbe stata chiesta.
    const auto snapshot = efis::SimulationProvider{5}.sample(
        efis::MonoTime{} + std::chrono::seconds{20}, std::chrono::seconds{20});
    const auto engine = efis::EngineSimulationProvider{5}.sample(
        efis::MonoTime{} + std::chrono::seconds{20}, std::chrono::seconds{20});

    const auto* airspeed_sample = efis::find_signal(snapshot,
                                                    efis::SignalId::indicated_airspeed);
    require(airspeed_sample != nullptr, "a signal the state carries must resolve");
    require(airspeed_sample->value == snapshot.air_data.indicated_airspeed_kt.value,
            "the lookup must return the field itself, not a copy of something else");
    require(airspeed_sample->origin.signal == efis::SignalId::indicated_airspeed,
            "the resolved sample must declare the signal that was asked for");

    // Chiedere allo stato sbagliato non da' un valore sbagliato: non da' nulla.
    require(efis::find_signal(snapshot, efis::SignalId::engine_rpm) == nullptr,
            "an engine quantity is not in the flight state");
    require(efis::find_signal(engine, efis::SignalId::roll) == nullptr,
            "a flight quantity is not in the engine state");
    require(efis::find_signal(snapshot, efis::SignalId::unknown) == nullptr,
            "the unknown signal must resolve to nothing");

    require(efis::find_signal(engine, efis::SignalId::cylinder_head_temperature_3) != nullptr,
            "each cylinder must be reachable on its own");
    require(efis::find_signal(engine, efis::SignalId::cylinder_head_temperature_3)->value ==
                engine.cht_c[2].value,
            "the third cylinder must resolve to the third cylinder, not another one");

    // L'elenco e la corrispondenza devono restare d'accordo. Se divergessero, il
    // menu offrirebbe strumenti che nessuno sa alimentare, oppure nasconderebbe
    // grandezze che il sistema produce. Non c'e' nulla che lo garantisca a
    // compilazione: lo garantisce questa prova.
    for (const efis::SignalId signal : efis::flight_signals()) {
        require(efis::find_signal(snapshot, signal) != nullptr,
                "every signal the menu offers must be reachable in the flight state");
    }
    for (const efis::SignalId signal : efis::engine_signals()) {
        require(efis::find_signal(engine, signal) != nullptr,
                "every signal the menu offers must be reachable in the engine state");
    }
    require(!efis::flight_signals().empty() && !efis::engine_signals().empty(),
            "the menus must not be empty");

    // Nessuna grandezza compare in entrambi gli elenchi: sarebbe ambiguo quale
    // stato interrogare.
    for (const efis::SignalId flight : efis::flight_signals()) {
        for (const efis::SignalId motor : efis::engine_signals()) {
            require(flight != motor,
                    "no quantity may belong to both states at once");
        }
    }

    // La regolazione barometrica e' raggiungibile come le altre, benche' non sia
    // una misura: la finestra di Kollsman e' uno strumento come gli altri.
    require(efis::find_signal(snapshot, efis::SignalId::baro_setting) != nullptr,
            "the altimeter setting must be reachable like any other quantity");


    // --- Altimetro a tre lancette --------------------------------------------
    //
    // La lettura errata di mille piedi e' l'incidente classico di questo
    // strumento, e nasce dal confondere la lancetta delle migliaia con quella
    // delle decine di migliaia. Le tre devono essere distinguibili per
    // costruzione, non per attenzione di chi guarda.
    const Sizing altimeter_size{480.0};
    const Dial at_5400 = describe_altimeter({valid(5400.0), "1013"}, altimeter_size);
    require(at_5400.needles.size() == 3, "an altimeter shows three needles");

    const Scale hundreds_scale{0.0, 1000.0, 0.0, 360.0, 100.0, 20.0, 0, 1000.0};
    const Scale thousands_scale{0.0, 10000.0, 0.0, 360.0, 1000.0, 200.0, 0, 10000.0};
    auto needle_of = [](const Dial& dial, NeedleKind kind) {
        for (const Needle& needle : dial.needles) {
            if (needle.kind == kind) return needle;
        }
        return Needle{};
    };
    require(close_to(needle_of(at_5400, NeedleKind::primary).angle_deg,
                     angle_for(hundreds_scale, 5400.0)),
            "the hundreds needle must show the remainder within the thousand");
    require(close_to(needle_of(at_5400, NeedleKind::secondary).angle_deg,
                     angle_for(thousands_scale, 5400.0)),
            "the thousands needle must show the remainder within ten thousand");

    // La piu' lunga e' quella che si muove piu' in fretta: e' la convenzione
    // degli strumenti veri, e invertirla renderebbe la lettura errata piu'
    // probabile invece che meno.
    require(needle_of(at_5400, NeedleKind::primary).length >
                needle_of(at_5400, NeedleKind::secondary).length,
            "the fastest needle must be the longest one");
    require(needle_of(at_5400, NeedleKind::secondary).length >
                needle_of(at_5400, NeedleKind::tertiary).length,
            "the slowest needle must be the shortest one");

    // Cento piedi piu' in su, la lancetta delle centinaia si sposta di un
    // decimo di giro esatto.
    const Dial at_5500 = describe_altimeter({valid(5500.0), "1013"}, altimeter_size);
    require(close_to(needle_of(at_5500, NeedleKind::primary).angle_deg -
                         needle_of(at_5400, NeedleKind::primary).angle_deg,
                     36.0),
            "one hundred feet must move the hundreds needle by a tenth of a turn");

    // La faccia riporta le cifre da 0 a 9, perche' la sua unita' e' il
    // centinaio di piedi. Scrivere 900 al posto di 9 renderebbe illeggibile
    // qualunque altimetro.
    require(has_label(at_5400, "9") && has_label(at_5400, "0"),
            "the altimeter face is numbered in hundreds of feet");
    require(!has_label(at_5400, "900"),
            "the face must not be numbered in feet, or it becomes unreadable");
    require(at_5400.labels.size() == 10, "the face carries ten figures, zero to nine");

    // La finestrella riporta la regolazione gia' formattata: l'unita' la sceglie
    // il pilota nel profilo, e lo strumento non deve saperne nulla.
    require(at_5400.window.present && at_5400.window.text == "1013",
            "the Kollsman window must show the setting");
    const Dial in_inches = describe_altimeter({valid(5400.0), "29.92"}, altimeter_size);
    require(in_inches.window.text == "29.92",
            "the window shows whatever unit the profile chose");

    // Finestrella vuota significa regolazione non disponibile. Mostrare 1013
    // direbbe che la regolazione e' standard, che e' un'affermazione diversa.
    const Dial no_setting = describe_altimeter({valid(5400.0), ""}, altimeter_size);
    require(!no_setting.window.present,
            "a missing setting leaves the window empty, it does not claim standard");

    // Dato assente o scaduto: nessuna lancetta, come su ogni altro quadrante.
    const Dial altimeter_failed = describe_altimeter(
        {{0.0, efis::Validity::unavailable, false}, "1013"}, altimeter_size);
    require(altimeter_failed.needles.empty(),
            "a failed altimeter must not show needles on zero");
    require(altimeter_failed.annunciation == Annunciation::failed,
            "a failed altimeter must say so");
    require(altimeter_failed.window.present,
            "the setting stays readable even when the altitude is not");

    // Il caso che rende pericoloso un altimetro di fronte a un dato corrotto.
    // Le lancette si avvolgono e non hanno estremi da cui uscire: un milione di
    // piedi si avvolgerebbe e mostrerebbe una quota bassa perfettamente
    // credibile. Fuori dai limiti di plausibilita' non si indica nulla.
    const Dial absurd = describe_altimeter({valid(1000000.0), "1013"}, altimeter_size);
    require(absurd.needles.empty(),
            "an implausible altitude must not be shown wrapped as a credible one");
    require(absurd.annunciation == Annunciation::out_of_range,
            "an implausible altitude must be declared out of range");
    const Dial deep = describe_altimeter({valid(-5000.0), "1013"}, altimeter_size);
    require(deep.needles.empty(), "an impossible depth must not be indicated either");

    // Sotto il livello del mare ma plausibile: si indica, perche' esistono
    // aeroporti sotto il livello del mare e la quota puo' essere negativa.
    const Dial below = describe_altimeter({valid(-200.0), "1013"}, altimeter_size);
    require(below.needles.size() == 3,
            "a plausible negative altitude must still be indicated");


    // --- Girodirezionale ------------------------------------------------------
    //
    // Qui non ruota la lancetta, ruota la rosa: la prua si legge sotto la linea
    // di fede fissa a ore dodici. Sbagliare il verso della rotazione produce uno
    // strumento che si legge benissimo e indica la prua specchiata.
    require(close_to(normalise_heading(370.0), 10.0),
            "headings wrap: three hundred seventy degrees is ten");
    require(close_to(normalise_heading(-10.0), 350.0),
            "a negative heading must wrap forwards, not stay negative");
    require(close_to(normalise_heading(360.0), 0.0), "a full turn is north");

    const Sizing card_size{480.0};
    auto label_angle = [](const Dial& dial, const std::string& text) {
        for (const Label& label : dial.labels) {
            if (label.text == text) return label.angle_deg;
        }
        return -1.0;
    };

    // Con prua nord la rosa e' ferma: N sotto la linea di fede, E a destra.
    const Dial north = describe_heading({valid(0.0), {}}, card_size);
    require(close_to(label_angle(north, "N"), 0.0),
            "heading north puts north under the lubber line");
    require(close_to(label_angle(north, "E"), 90.0),
            "east must sit to the right when heading north");
    require(close_to(label_angle(north, "W"), 270.0),
            "west must sit to the left when heading north");

    // Virando a est la rosa gira in senso opposto: E finisce sotto la linea di
    // fede e N si sposta a sinistra. E' il verso che distingue una bussola da
    // uno strumento specchiato.
    const Dial east = describe_heading({valid(90.0), {}}, card_size);
    require(close_to(label_angle(east, "E"), 0.0),
            "heading east puts east under the lubber line");
    require(close_to(label_angle(east, "N"), 270.0),
            "north must move to the left as the aircraft turns right");
    require(close_to(label_angle(east, "S"), 90.0),
            "south must move to the right as the aircraft turns right");

    // La linea di fede appartiene al quadrante, non alla misura.
    auto has_kind = [](const Dial& dial, NeedleKind kind) {
        for (const Needle& needle : dial.needles) {
            if (needle.kind == kind) return true;
        }
        return false;
    };
    require(has_kind(north, NeedleKind::reference),
            "the card carries a fixed lubber line");

    // Prua assente: nessuna rosa. Una rosa ferma sarebbe la riproduzione fedele
    // di un girodirezionale guasto, ed e' proprio il guasto che non deve poter
    // essere scambiato per una misura.
    const Dial lost = describe_heading({{0.0, efis::Validity::unavailable, false}, {}},
                                       card_size);
    require(lost.labels.empty() && lost.ticks.empty(),
            "a failed heading must not leave a card that reads as north");
    require(lost.annunciation == Annunciation::failed,
            "a failed heading must say so");
    require(has_kind(lost, NeedleKind::reference),
            "the lubber line stays: it shows where the heading would be read");

    // La prua selezionata non e' una misura e non sparisce con essa.
    const Dial with_bug = describe_heading({valid(0.0), valid(45.0)}, card_size);
    require(has_kind(with_bug, NeedleKind::bug), "a selected heading must be shown");
    for (const Needle& needle : with_bug.needles) {
        if (needle.kind != NeedleKind::bug) continue;
        require(close_to(needle.angle_deg, 45.0),
                "the bug sits at the selected heading relative to the card");
    }

    // Senza prua l indice non puo essere collocato, e non e una limitazione:
    // la sua posizione e definita rispetto alla rosa, e sottrarre una prua che
    // non esiste darebbe un angolo che non significa nulla. Cio che il pilota ha
    // scelto non e perduto, semplicemente non ha piu un posto su questo
    // quadrante.
    const Dial bug_only = describe_heading({{0.0, efis::Validity::unavailable, false},
                                            valid(45.0)}, card_size);
    require(!has_kind(bug_only, NeedleKind::bug),
            "without a card the bug has no meaningful place to sit");

    // La rosa si dirada come i quadranti a scala: a 240 px sopravvivono meno
    // scritte, ma i punti cardinali restano.
    const Dial small_card = describe_heading({valid(0.0), {}}, Sizing{240.0});
    require(small_card.labels.size() <= north.labels.size(),
            "a smaller card must not carry more figures");
    require(!small_card.labels.empty() && !small_card.ticks.empty(),
            "even the smallest card keeps a usable graduation");

    // La graduazione copre il giro senza ripetere lo zero: una tacca a 360
    // cadrebbe sopra quella a 0.
    for (const Label& label : north.labels) {
        require(label.angle_deg >= 0.0 && label.angle_deg < 360.0,
                "every figure must sit within one turn");
    }


    // --- Orizzonte artificiale ------------------------------------------------
    //
    // Il quadro ruota in senso opposto al rollio e trasla col beccheggio: e' il
    // mondo a inclinarsi, non l aeroplano. Invertire uno dei due versi produce
    // uno strumento che si legge benissimo e mostra l assetto specchiato.
    const Sizing horizon_size{480.0};

    const Horizon level = describe_attitude({valid(0.0), valid(0.0)}, horizon_size);
    require(level.has_attitude, "a valid attitude must be shown");
    require(close_to(level.rotation_deg, 0.0), "wings level means no rotation");
    require(close_to(level.translation, 0.0), "level flight means no translation");

    // Rollio a destra: il quadro ruota a sinistra della stessa quantita.
    const Horizon banked = describe_attitude({valid(30.0), valid(0.0)}, horizon_size);
    require(close_to(banked.rotation_deg, -30.0),
            "a right bank must rotate the world to the left");
    require(close_to(banked.bank_pointer_deg, 30.0),
            "the bank pointer follows the aircraft, not the world");

    // A cabrare l orizzonte scende, quindi il quadro trasla verso il basso.
    const Horizon climbing = describe_attitude({valid(0.0), valid(10.0)}, horizon_size);
    require(climbing.translation > 0.0,
            "pitching up must move the horizon down the instrument");
    const Horizon diving = describe_attitude({valid(0.0), valid(-10.0)}, horizon_size);
    require(diving.translation < 0.0,
            "pitching down must move the horizon up the instrument");
    require(close_to(climbing.translation, -diving.translation),
            "equal pitch up and down must move the horizon equally");

    // Il rollio si avvolge invece di uscire di scala: oltre i centottanta gradi
    // l aeroplano e rovescio, non fuori limite.
    const Horizon inverted = describe_attitude({valid(200.0), valid(0.0)}, horizon_size);
    require(inverted.has_attitude, "an inverted aircraft still has an attitude");
    require(close_to(inverted.rotation_deg, 160.0),
            "roll must wrap to the shorter way round");

    // La scala di beccheggio ha un gradino ogni cinque gradi, lungo ogni dieci,
    // e l orizzonte non e un gradino.
    require(!level.ladder.empty(), "the pitch ladder must be drawn");
    for (const Rung& rung : level.ladder) {
        require(std::abs(rung.pitch_deg) > 1e-9,
                "the horizon itself is not a rung of the ladder");
    }
    std::size_t labelled = 0;
    for (const Rung& rung : level.ladder) {
        if (!rung.text.empty()) ++labelled;
    }
    require(labelled > 0, "the ten degree rungs must carry their figure");
    require(labelled < level.ladder.size(),
            "the five degree rungs must stay unlabelled, or the ladder is unreadable");

    // La scala si dirada con la dimensione, come le tacche dei quadranti: su un
    // display circolare piccolo i gradini ogni cinque gradi si toccherebbero.
    const Horizon small_horizon = describe_attitude({valid(0.0), valid(0.0)},
                                                    Sizing{240.0});
    require(small_horizon.ladder.size() <= level.ladder.size(),
            "a smaller horizon must not carry a denser ladder");
    require(!small_horizon.ladder.empty(),
            "even the smallest horizon keeps a usable ladder");
    require(small_horizon.has_attitude,
            "thinning the ladder must not remove the attitude");

    // Assetto assente: nessun quadro. Un orizzonte fermo e la riproduzione
    // fedele di un indicatore guasto, e si legge come volo livellato e ali
    // orizzontali.
    const Horizon dead = describe_attitude(
        {{0.0, efis::Validity::unavailable, false}, valid(0.0)}, horizon_size);
    require(!dead.has_attitude,
            "a failed attitude must not leave a level horizon on the glass");
    require(dead.ladder.empty(), "no attitude means no ladder either");
    require(dead.annunciation == Annunciation::failed, "a failed attitude must say so");
    require(!dead.bank_scale.empty(),
            "the bank scale belongs to the bezel and stays");

    // Rollio e beccheggio vengono dalla stessa piattaforma: basta che uno manchi.
    const Horizon half_dead = describe_attitude(
        {valid(0.0), {0.0, efis::Validity::unavailable, false}}, horizon_size);
    require(!half_dead.has_attitude,
            "attitude needs both angles: half of it is not an attitude");

    // Beccheggio impossibile: nessun aeromobile supera la verticale, e disegnarlo
    // farebbe uscire il quadro dallo strumento senza che nulla lo dichiari.
    const Horizon absurd_pitch = describe_attitude({valid(0.0), valid(120.0)},
                                                   horizon_size);
    require(!absurd_pitch.has_attitude, "a pitch beyond vertical must not be drawn");
    require(absurd_pitch.annunciation == Annunciation::out_of_range,
            "a pitch beyond vertical must be declared out of range");

    // --- Virosbandometro ------------------------------------------------------
    //
    // Due grandezze indipendenti, ed e la loro indipendenza a renderlo utile:
    // una virata puo essere giusta e scoordinata, o coordinata e sbagliata.
    const TurnCoordinator straight = describe_turn({valid(0.0), valid(0.0)}, horizon_size);
    require(straight.has_turn && straight.has_ball,
            "both quantities present means both are shown");
    require(close_to(straight.aircraft_bank_deg, 0.0),
            "no turn means the aircraft symbol stays level");
    require(close_to(straight.ball_offset, 0.0),
            "a coordinated turn keeps the ball centred");

    // Virata a destra: la sagoma si inclina a destra, in proporzione al rateo.
    const TurnCoordinator right = describe_turn({valid(3.0), valid(0.0)}, horizon_size);
    require(right.aircraft_bank_deg > 0.0, "turning right must bank the symbol right");
    const TurnCoordinator left = describe_turn({valid(-3.0), valid(0.0)}, horizon_size);
    require(close_to(left.aircraft_bank_deg, -right.aircraft_bank_deg),
            "the indication must be symmetric");
    const TurnCoordinator faster = describe_turn({valid(6.0), valid(0.0)}, horizon_size);
    require(faster.aircraft_bank_deg > right.aircraft_bank_deg,
            "a faster turn must bank the symbol further");

    // La virata standard e il riferimento per cui il quadrante esiste: alla
    // tacca ci si arriva esattamente a tre gradi al secondo.
    require(straight.standard_marks.size() == 2,
            "there is a standard rate mark on each side");
    require(close_to(right.aircraft_bank_deg, straight.standard_marks[1].angle_deg),
            "three degrees per second must put the symbol on the standard mark");

    // Oltre il limite del quadrante la sagoma resta al limite: farla ruotare
    // oltre la verticale renderebbe illeggibile proprio la virata piu violenta.
    const TurnCoordinator violent = describe_turn({valid(30.0), valid(0.0)}, horizon_size);
    require(close_to(violent.aircraft_bank_deg, faster.aircraft_bank_deg),
            "beyond the scale the symbol stops, it does not keep rotating");

    // La pallina esce verso l esterno della virata quando si scivola.
    const TurnCoordinator slipping = describe_turn({valid(3.0), valid(0.1)}, horizon_size);
    require(slipping.ball_offset > 0.0, "lateral acceleration must move the ball");
    const TurnCoordinator hard_slip = describe_turn({valid(3.0), valid(5.0)}, horizon_size);
    require(close_to(hard_slip.ball_offset, 1.0),
            "the ball stops at the end of the tube, it does not leave it");

    // Le due grandezze si guastano separatamente, e lo strumento conserva meta
    // della propria utilita: dichiararlo guasto per intero la butterebbe via.
    const TurnCoordinator no_gyro = describe_turn(
        {{0.0, efis::Validity::unavailable, false}, valid(0.0)}, horizon_size);
    require(!no_gyro.has_turn, "a lost turn rate removes the aircraft symbol");
    require(no_gyro.has_ball, "the ball does not depend on the gyro");
    require(no_gyro.annunciation == Annunciation::none,
            "half an instrument is still half useful, and must not be declared dead");

    const TurnCoordinator nothing = describe_turn(
        {{0.0, efis::Validity::unavailable, false},
         {0.0, efis::Validity::unavailable, false}}, horizon_size);
    require(!nothing.has_turn && !nothing.has_ball,
            "with neither quantity nothing is indicated");
    require(nothing.annunciation == Annunciation::failed,
            "only when both are gone is the instrument declared failed");
    require(!nothing.standard_marks.empty(),
            "the standard rate marks belong to the dial and stay");


    // --- Provenienza dei limiti ----------------------------------------------
    //
    // Una fascia verde dice «qui si puo volare», ed e una dichiarazione di
    // sicurezza. Se nessuno l ha verificata sul manuale, lo strumento deve dirlo
    // invece di lasciar credere che qualcuno l abbia firmata.
    const std::vector<BandRange> ranges{{45.0, 120.0, Band::green}};

    const Dial unverified = describe(asi, valid(95.0), panel,
                                     Limits{ranges, LimitsProvenance::indicative});
    require(unverified.arcs.size() == 1, "indicative limits are still drawn");
    require(unverified.limits == LimitsProvenance::indicative,
            "the dial must declare where its bands came from");

    const Dial checked = describe(asi, valid(95.0), panel,
                                  Limits{ranges, LimitsProvenance::verified});
    require(checked.limits == LimitsProvenance::verified,
            "verified limits must be declared as such");
    require(checked.arcs.size() == unverified.arcs.size(),
            "verification changes what is declared, not what is drawn");

    // Il caso che conta: senza limiti configurati non si disegna nulla, anche se
    // qualcuno passasse delle fasce. Un quadrante senza archi dichiara di non
    // sapere dove siano i limiti; uno con archi inventati afferma di saperlo.
    const Dial no_limits = describe(asi, valid(95.0), panel,
                                    Limits{ranges, LimitsProvenance::absent});
    require(no_limits.arcs.empty(),
            "bands with no provenance must not be drawn at all");
    require(no_limits.limits == LimitsProvenance::absent,
            "the absence of limits must itself be declared");
    require(!no_limits.needles.empty(),
            "missing limits do not stop the instrument from indicating");

    // Il valore prudente e quello di partenza: chi dimentica di dichiarare la
    // provenienza non ottiene fasce, invece di ottenerle senza avvertimento.
    const Dial defaulted = describe(asi, valid(95.0), panel, Limits{ranges});
    require(defaulted.arcs.empty(),
            "the cautious value must be the default, not the convenient one");

    // --- Quadrante a piu lancette omogenee ------------------------------------
    //
    // Su un gruppo di cilindri l informazione utile non e quanto valga ciascuno,
    // e quanto si scostino fra loro.
    const Scale cht{100.0, 260.0, -150.0, 300.0, 20.0, 5.0, 0, 0.0};
    const Limits cht_limits{{{100.0, 220.0, Band::green}, {220.0, 260.0, Band::red}},
                            LimitsProvenance::indicative};

    const Dial four = describe_cluster(
        cht, {{valid(178.0), valid(181.0), valid(184.0), valid(187.0)}, false},
        panel, cht_limits);
    require(four.needles.size() == 4, "four cylinders means four needles");
    require(four.annunciation == Annunciation::none,
            "four healthy cylinders raise nothing");

    // Tutte le lancette hanno lo stesso peso: fra quattro cilindri non esiste
    // una gerarchia, e darne una a uno solo la suggerirebbe.
    for (const Needle& needle : four.needles) {
        require(needle.kind == NeedleKind::primary,
                "homogeneous channels must not be drawn with different ranks");
    }

    // Le lancette seguono l ordine dei canali: il terzo cilindro deve stare dove
    // ci si aspetta il terzo cilindro.
    require(close_to(four.needles[2].angle_deg, angle_for(cht, 184.0)),
            "each needle must sit at its own channel value");

    // Un canale perduto non spegne lo strumento, ma degrada il confronto per cui
    // esiste, e va detto.
    const Dial three_cylinders = describe_cluster(
        cht, {{valid(178.0), {0.0, efis::Validity::unavailable, false},
               valid(184.0), valid(187.0)}, false},
        panel, cht_limits);
    require(three_cylinders.needles.size() == 3, "a lost channel removes only its own needle");
    require(three_cylinders.annunciation == Annunciation::partial,
            "a partly working cluster must declare it");
    require(std::string{annunciation_text(Annunciation::partial)} == "PARZIALE",
            "the partial wording must stay stable");

    // Parziale ha la precedenza sul fuori scala: sapere che manca un cilindro
    // conta piu che sapere che un altro e alto.
    const Dial hot_and_partial = describe_cluster(
        cht, {{valid(300.0), {0.0, efis::Validity::unavailable, false},
               valid(184.0), valid(187.0)}, false},
        panel, cht_limits);
    require(hot_and_partial.annunciation == Annunciation::partial,
            "a missing channel matters more than a high one");

    // Tutti i canali perduti: lo strumento non ha nulla da mostrare.
    const efis::instruments::Reading gone{0.0, efis::Validity::unavailable, false};
    const Dial none_left = describe_cluster(cht, {{gone, gone, gone, gone}, false},
                                            panel, cht_limits);
    require(none_left.needles.empty(), "with no channel there is nothing to indicate");
    require(none_left.annunciation == Annunciation::failed,
            "a cluster with no channel at all is failed, not partial");
    require(!none_left.ticks.empty(), "the dial face stays");

    // Scaduto resta distinto da assente anche qui.
    const efis::instruments::Reading old_value{180.0, efis::Validity::stale, false};
    const Dial all_stale = describe_cluster(
        cht, {{old_value, old_value, old_value, old_value}, false}, panel, cht_limits);
    require(all_stale.annunciation == Annunciation::stale,
            "stale channels must not be reported as missing");

    // Fuori scala con tutti i canali presenti.
    const Dial too_hot = describe_cluster(
        cht, {{valid(178.0), valid(181.0), valid(184.0), valid(400.0)}, false},
        panel, cht_limits);
    require(too_hot.annunciation == Annunciation::out_of_range,
            "a channel beyond the scale must be declared");
    require(too_hot.needles.size() == 4, "the other channels keep indicating");

    // Due serbatoi sullo stesso quadrante: e lo stesso meccanismo dei cilindri.
    const Scale fuel{0.0, 20.0, -150.0, 300.0, 5.0, 1.0, 0, 0.0};
    const Dial tanks = describe_cluster(fuel, {{valid(18.0), valid(11.0)}, false},
                                        panel, Limits{});
    require(tanks.needles.size() == 2, "two tanks means two needles");
    require(tanks.arcs.empty(),
            "with no configured limits the fuel gauge shows no bands");


    // --- Pannello configurabile ------------------------------------------------
    //
    // Le regole che decidono se una disposizione sia accettabile sono regole di
    // sicurezza, non di presentazione. Una pagina che promette uno strumento
    // senza sorgente, o che schiaccia sei quadranti dove non si leggono, il
    // pilota la scopre in volo.
    // La soglia di leggibilita e quella predefinita: 240 px, la dimensione piu
    // piccola a cui gli strumenti sono stati davvero guardati.
    const Surface panel_1024{1024.0, 600.0};

    const PanelLayout six = classic_six_pack();
    require(validate(six, panel_1024).accepted(),
            "the classic six pack must be valid at the panel resolution");
    require(six.pages.size() == 1 && six.pages[0].slots.size() == 6,
            "the six pack has six instruments on one page");

    auto has_problem = [](const ValidationResult& result, LayoutProblem problem) {
        for (const LayoutIssue& issue : result.issues) {
            if (issue.problem == problem) return true;
        }
        return false;
    };

    // Ogni motivo di rifiuto va detto per esteso: «configurazione non valida»
    // non permette a nessuno di correggerla.
    require(std::string{layout_problem_text(LayoutProblem::signal_unavailable)} ==
                "GRANDEZZA_ASSENTE",
            "the reasons must be stated in words that can be acted on");

    // Due strumenti nella stessa posizione: uno coprirebbe l altro.
    PanelLayout overlapping = classic_six_pack();
    overlapping.pages[0].slots.push_back({0, 0, InstrumentKind::vertical_speed});
    const auto overlap_result = validate(overlapping, panel_1024);
    require(!overlap_result.accepted() &&
                has_problem(overlap_result, LayoutProblem::overlapping_slots),
            "two instruments in one place must be refused");

    // La posizione del problema viene indicata, invece di lasciarla cercare.
    bool located = false;
    for (const LayoutIssue& issue : overlap_result.issues) {
        if (issue.problem != LayoutProblem::overlapping_slots) continue;
        if (issue.page == 0 && issue.row == 0 && issue.column == 0) located = true;
    }
    require(located, "the problem must say where it is");

    // Strumento fuori dalla griglia dichiarata.
    PanelLayout beyond_grid = classic_six_pack();
    beyond_grid.pages[0].slots.push_back({5, 9, InstrumentKind::airspeed});
    require(has_problem(validate(beyond_grid, panel_1024), LayoutProblem::slot_outside_grid),
            "a slot outside the grid must be refused");

    // Griglia senza righe.
    PanelLayout no_grid = classic_six_pack();
    no_grid.pages[0].rows = 0;
    require(has_problem(validate(no_grid, panel_1024), LayoutProblem::empty_grid),
            "a grid with no rows must be refused");

    // Tipo sconosciuto: viene da una versione piu recente, oppure il file e
    // stato modificato a mano. In entrambi i casi non si indovina.
    PanelLayout unknown = classic_six_pack();
    unknown.pages[0].slots[0].kind = static_cast<InstrumentKind>(60000);
    require(has_problem(validate(unknown, panel_1024), LayoutProblem::unknown_instrument),
            "an unknown instrument must be refused, not guessed");

    // Disposizione vuota.
    PanelLayout empty;
    require(has_problem(validate(empty, panel_1024), LayoutProblem::empty_layout),
            "a layout with no instruments must be refused");

    // Leggibilita. Gli stessi sei strumenti su un display circolare piccolo non
    // ci stanno: e il vincolo che la classe Round ha imposto fin dall inizio.
    const Surface round_480{480.0, 480.0};
    require(!validate(six, round_480).accepted(),
            "six instruments do not fit on a small round display");
    require(has_problem(validate(six, round_480), LayoutProblem::illegible_at_resolution),
            "the refusal must name legibility, not something else");

    // Un solo strumento sulla stessa superficie ci sta benissimo.
    PanelLayout single;
    PanelPage one;
    one.name = "Singolo";
    one.rows = 1;
    one.columns = 1;
    one.slots = {{0, 0, InstrumentKind::airspeed}};
    single.pages.push_back(one);
    require(validate(single, round_480).accepted(),
            "one instrument on a round display is exactly what it is for");

    // Capability. Il paragrafo 18 vuole una sola codebase con differenze per
    // capability, non per fork: uno strumento che richiede la catena EMS non
    // compare su un profilo Standard.
    PanelLayout standard_with_egt = classic_six_pack();
    standard_with_egt.pages[0].slots[0].kind = InstrumentKind::exhaust_gas_temperature;
    require(has_problem(validate(standard_with_egt, panel_1024),
                        LayoutProblem::capability_required),
            "an instrument beyond the profile must be refused");

    standard_with_egt.capability = Capability::professional;
    require(validate(standard_with_egt, panel_1024).accepted(),
            "the same instrument is fine once the profile allows it");

    require(validate(engine_page(), panel_1024).accepted(),
            "the engine page must be valid on a professional profile");

    // Catalogo. Ogni strumento offerto deve avere le proprie grandezze
    // effettivamente disponibili, altrimenti il menu promette cio' che il
    // pannello non puo' mantenere.
    require(!instrument_catalogue().empty(), "the menu must offer something");
    for (const InstrumentDefinition& entry : instrument_catalogue()) {
        require(find_instrument(entry.kind) == &entry,
                "every catalogued instrument must be findable by its kind");
        require(!entry.required.empty(),
                "an instrument with no source is not an instrument");
        for (const efis::SignalId signal : entry.required) {
            const bool in_flight = [signal] {
                for (const efis::SignalId candidate : efis::flight_signals()) {
                    if (candidate == signal) return true;
                }
                return false;
            }();
            const bool in_engine = [signal] {
                for (const efis::SignalId candidate : efis::engine_signals()) {
                    if (candidate == signal) return true;
                }
                return false;
            }();
            require(in_flight || in_engine,
                    "the menu must not offer an instrument whose source does not exist");
        }
    }
    require(find_instrument(static_cast<InstrumentKind>(60000)) == nullptr,
            "an unknown kind must not resolve to a definition");


    // --- Persistenza del pannello ---------------------------------------------
    //
    // Stessa disciplina del profilo aeromobile, che il paragrafo 5 impone a ogni
    // configurazione critica: si valida, si attiva in modo atomico lasciando una
    // copia e una riga di registro, e si puo' tornare indietro.
    const auto panel_dir = std::filesystem::temp_directory_path() / "efis-panel-tests";
    std::filesystem::remove_all(panel_dir);
    const PanelPaths paths{panel_dir / "active" / "panel.json",
                           panel_dir / "backups",
                           panel_dir / "audit" / "panel-audit.log"};

    // Andata e ritorno: cio' che si scrive si deve poter rileggere identico,
    // altrimenti la configurazione salvata non e' quella che il pilota ha
    // scelto.
    const PanelLayout original = classic_six_pack();
    const ParseResult reread = from_json(to_json(original));
    require(reread.ok(), "what was written must be readable again");
    require(reread.layout->pages.size() == original.pages.size(),
            "the number of pages must survive the round trip");
    require(reread.layout->pages[0].slots.size() == 6,
            "the instruments must survive the round trip");
    require(reread.layout->pages[0].slots[1].kind == InstrumentKind::attitude,
            "each instrument must come back in its own place");
    require(reread.layout->pages[0].name == original.pages[0].name,
            "the page name must survive");

    // Il lettore e' severo di proposito. Uno permissivo accetterebbe un file
    // quasi giusto e produrrebbe una disposizione quasi giusta, che e' la
    // condizione peggiore: il pannello si apre e manca qualcosa.
    require(!from_json("").ok(), "empty text is not a layout");
    require(from_json("").problem == ParseProblem::malformed,
            "empty text must be reported as malformed");
    require(!from_json("{\"pages\": []}").ok(),
            "a layout without a schema version must be refused");
    require(from_json("{\"pages\": []}").problem == ParseProblem::missing_field,
            "the missing field must be named as such");

    // Uno schema piu' recente non si interpreta: indovinare che cosa siano i
    // campi nuovi produrrebbe una disposizione plausibile e sbagliata.
    const ParseResult future = from_json("{\"schema_version\": 99, \"pages\": []}");
    require(!future.ok(), "a newer schema must not be guessed at");
    require(future.problem == ParseProblem::unsupported_schema,
            "the refusal must name the schema, not something else");

    // Una chiave sconosciuta non si ignora: se qualcuno l ha scritta si aspetta
    // che conti.
    const ParseResult surprise =
        from_json("{\"schema_version\": 1, \"sorpresa\": 3, \"pages\": []}");
    require(!surprise.ok(), "an unknown key must be refused, not ignored");
    require(surprise.field == "sorpresa", "the refusal must name the key it found");

    // Un tipo sbagliato viene detto per quello che e'.
    const ParseResult wrong =
        from_json("{\"schema_version\": 1, \"profile_id\": 7, \"pages\": []}");
    require(wrong.problem == ParseProblem::wrong_type,
            "a field of the wrong type must be reported as such");
    require(std::string{parse_problem_text(ParseProblem::wrong_type)} == "TIPO_ERRATO",
            "the wording must stay stable");

    // Attivazione. Prima non c'e' nulla da leggere.
    PanelStore store{paths};
    require(!store.load().ok(), "with no file there is nothing to load");

    const ActivationReport first = store.activate(original, panel_1024);
    require(first.outcome == ActivationOutcome::activated,
            "a valid layout must be activated");
    require(std::filesystem::exists(paths.active), "the active file must exist");
    require(first.backup.empty(),
            "the first activation has nothing to back up");
    require(store.load().ok(), "what was activated must be loadable");

    // Una disposizione rifiutata non tocca nulla. Non deve poter sostituire una
    // funzionante nemmeno per un istante.
    PanelLayout broken = classic_six_pack();
    broken.pages[0].slots.push_back({0, 0, InstrumentKind::vertical_speed});
    const ActivationReport refused = store.activate(broken, panel_1024);
    require(refused.outcome == ActivationOutcome::rejected,
            "an invalid layout must be refused");
    require(!refused.validation.accepted(),
            "the refusal must carry the reason");
    const ParseResult after_refusal = store.load();
    require(after_refusal.ok() && after_refusal.layout->pages[0].slots.size() == 6,
            "a refused activation must leave the previous layout untouched");

    // Seconda attivazione: lascia una copia della precedente.
    PanelLayout second = classic_six_pack();
    second.revision = 2;
    second.pages[0].slots[5].kind = InstrumentKind::tachometer;
    const ActivationReport again = store.activate(second, panel_1024);
    require(again.outcome == ActivationOutcome::activated, "the second must activate too");
    require(!again.backup.empty() && std::filesystem::exists(again.backup),
            "activating over an existing layout must leave a backup");
    require(store.load().layout->revision == 2, "the new revision must be active");

    // Si puo' tornare indietro.
    require(store.rollback(), "the previous layout must be restorable");
    require(store.load().layout->revision == 1,
            "after rollback the previous revision is active again");
    require(store.load().layout->pages[0].slots[5].kind == InstrumentKind::vertical_speed,
            "rollback must restore the instruments, not just the number");

    // Il registro di controllo ha visto passare tutto: attivazioni, rifiuto e
    // ripristino. Senza registro un ripristino non si distingue da una modifica.
    require(std::filesystem::exists(paths.audit), "every attempt must leave a record");
    // Il registro si legge dentro un blocco, cosi' il file e' gia' chiuso quando
    // piu' sotto la directory di prova viene rimossa. Su Windows un file ancora
    // aperto non si cancella e remove_all lancia: una prova che si porta dietro
    // un descrittore aperto non e' portabile.
    const std::string audit_text = [&paths] {
        std::ifstream audit_file{paths.audit};
        return std::string{std::istreambuf_iterator<char>{audit_file},
                           std::istreambuf_iterator<char>{}};
    }();
    require(audit_text.find("ATTIVATA") != std::string::npos,
            "the record must show the activations");
    require(audit_text.find("RIFIUTATA") != std::string::npos,
            "the record must show the refusal too, not only the successes");
    require(audit_text.find("RIPRISTINATA") != std::string::npos,
            "the record must show the rollback");

    // Senza copia di sicurezza non si torna indietro, e lo si dice.
    PanelStore fresh{{panel_dir / "altro" / "panel.json", panel_dir / "altro-backup",
                      panel_dir / "altro" / "audit.log"}};
    require(!fresh.rollback(), "with no backup there is nothing to restore");

    std::filesystem::remove_all(panel_dir);


    std::cout << "All EFIS instrument tests passed\n";
}
