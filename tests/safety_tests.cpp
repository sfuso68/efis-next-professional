// Prove delle protezioni contro il dato plausibile ma falso.
//
// Sono separate dalle prove del nucleo perche' verificano una categoria diversa:
// non che il sistema calcoli il valore giusto, ma che si accorga quando il
// valore che riceve non significa piu' nulla. Un guasto qui non produce un
// numero sbagliato di poco, produce un numero fermo e convincente.

#include "efis/core/cross_check.hpp"
#include "efis/core/stasis_monitor.hpp"
#include "efis/core/flight_model.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

efis::Sample<double> reading(double value, efis::SignalId signal,
                             efis::MonoTime at,
                             efis::Validity validity = efis::Validity::valid) {
    efis::Sample<double> sample{};
    sample.value = value;
    sample.raw = value;
    sample.source = efis::SourceId::pixhawk_primary;
    sample.sampled_at = at;
    sample.received_at = at;
    sample.quality = efis::Quality::nominal;
    sample.validity = validity;
    sample.origin.signal = signal;
    return sample;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};

    // --- Valore fermo ---------------------------------------------------------

    efis::StasisMonitor monitor;

    // Un segnale senza politica non viene sorvegliato. Applicare una soglia
    // inventata sarebbe peggio che non sorvegliare: produrrebbe allarmi il cui
    // significato nessuno saprebbe interpretare.
    require(monitor.observe(reading(100.0, efis::SignalId::indicated_airspeed, epoch),
                            epoch) == efis::Motion::unknown,
            "a signal without a configured policy must not be judged");

    monitor.configure(efis::SignalId::indicated_airspeed,
                      {.suspect_after = 5000ms, .resolution = 0.5, .applicable = true});

    // La prima osservazione non puo' dire nulla: serve un termine di paragone.
    require(monitor.observe(reading(80.0, efis::SignalId::indicated_airspeed, epoch),
                            epoch) == efis::Motion::unknown,
            "the first observation cannot decide anything");

    // Un valore che si muove resta tale.
    require(monitor.observe(reading(82.0, efis::SignalId::indicated_airspeed,
                                    epoch + 1s), epoch + 1s) == efis::Motion::moving,
            "a changing value must be reported as moving");

    // Il caso che conta. Il campione continua ad arrivare fresco, con validita'
    // piena: ogni singolo controllo esistente lo considera buono. Solo il valore
    // non cambia piu'. E' il pitot ostruito.
    auto frozen_time = epoch + 2s;
    efis::Motion verdict = efis::Motion::unknown;
    for (int step = 0; step < 8; ++step) {
        frozen_time += 1s;
        const auto sample = reading(82.0, efis::SignalId::indicated_airspeed, frozen_time);
        require(sample.usable(),
                "the frozen sample must stay valid: that is what makes it dangerous");
        verdict = monitor.observe(sample, frozen_time);
    }
    require(verdict == efis::Motion::suspect,
            "a value that stops changing while still arriving must be flagged");

    // Prima della soglia non si grida.
    efis::StasisMonitor patient;
    patient.configure(efis::SignalId::pressure_altitude,
                      {.suspect_after = 5000ms, .resolution = 1.0, .applicable = true});
    patient.observe(reading(1500.0, efis::SignalId::pressure_altitude, epoch), epoch);
    require(patient.observe(reading(1500.0, efis::SignalId::pressure_altitude,
                                    epoch + 3s), epoch + 3s) == efis::Motion::moving,
            "stillness shorter than the threshold must not raise suspicion");
    require(patient.observe(reading(1500.0, efis::SignalId::pressure_altitude,
                                    epoch + 6s), epoch + 6s) == efis::Motion::suspect,
            "stillness beyond the threshold must raise suspicion");

    // Una variazione sotto la risoluzione dichiarata non e' movimento: un
    // sensore quantizzato ripete lo stesso numero, e un rumore di un bit non
    // dimostra che il sensore stia misurando.
    efis::StasisMonitor quantised;
    quantised.configure(efis::SignalId::pressure_altitude,
                        {.suspect_after = 5000ms, .resolution = 1.0, .applicable = true});
    quantised.observe(reading(1500.0, efis::SignalId::pressure_altitude, epoch), epoch);
    quantised.observe(reading(1500.4, efis::SignalId::pressure_altitude, epoch + 3s),
                      epoch + 3s);
    require(quantised.observe(reading(1500.6, efis::SignalId::pressure_altitude,
                                      epoch + 6s), epoch + 6s) == efis::Motion::suspect,
            "changes below the declared resolution must not count as movement");

    // Una variazione vera azzera il conteggio.
    require(quantised.observe(reading(1510.0, efis::SignalId::pressure_altitude,
                                      epoch + 7s), epoch + 7s) == efis::Motion::moving,
            "a real change must clear the suspicion");
    require(quantised.observe(reading(1510.0, efis::SignalId::pressure_altitude,
                                      epoch + 9s), epoch + 9s) == efis::Motion::moving,
            "after a real change the timer must start again from there");

    // Alcune grandezze restano ferme per tutto il volo senza che significhi
    // nulla. Per quelle il rilevamento si disattiva, invece di tararlo su una
    // soglia altissima che sarebbe comunque sbagliata.
    efis::StasisMonitor selected;
    selected.configure(efis::SignalId::selected_altitude,
                       {.suspect_after = 1000ms, .resolution = 0.0, .applicable = false});
    selected.observe(reading(5500.0, efis::SignalId::selected_altitude, epoch), epoch);
    require(selected.observe(reading(5500.0, efis::SignalId::selected_altitude,
                                     epoch + 60s), epoch + 60s) == efis::Motion::unknown,
            "a legitimately constant quantity must not be watched at all");

    // Un dato gia' scaduto non va sorvegliato: la sua immobilita' discende dal
    // guasto gia' rilevato. Contarla darebbe due segnalazioni per una causa
    // sola, e la seconda sarebbe fuorviante.
    efis::StasisMonitor after_loss;
    after_loss.configure(efis::SignalId::indicated_airspeed,
                         {.suspect_after = 1000ms, .resolution = 0.5, .applicable = true});
    after_loss.observe(reading(80.0, efis::SignalId::indicated_airspeed, epoch), epoch);
    require(after_loss.observe(reading(80.0, efis::SignalId::indicated_airspeed,
                                       epoch + 5s, efis::Validity::stale),
                               epoch + 5s) == efis::Motion::unknown,
            "an already stale value must not also be reported as frozen");

    // Ripreso il flusso, la sorveglianza riparte da capo e non eredita il
    // silenzio precedente come se fosse stasi.
    require(after_loss.observe(reading(80.0, efis::SignalId::indicated_airspeed,
                                       epoch + 6s), epoch + 6s) == efis::Motion::unknown,
            "after a loss the watch must restart, not inherit the silence");

    require(std::string{efis::motion_name(efis::Motion::suspect)} == "SUSPECT",
            "motion names must stay stable for recorded data");

    // --- Confronto incrociato -------------------------------------------------
    //
    // Due catene fisicamente separate non si guastano allo stesso modo nello
    // stesso istante. Se divergono, una delle due mente, e l'informazione utile
    // e' proprio che non si sa quale.

    efis::CrossCheck vertical{{.tolerance = 300.0, .dwell = 3000ms}};

    auto baro = reading(500.0, efis::SignalId::vertical_speed, epoch);
    auto satellite = reading(520.0, efis::SignalId::vertical_speed, epoch);
    require(vertical.compare(baro, satellite, epoch).agreement == efis::Agreement::agree,
            "two chains within tolerance must agree");

    // Uno scostamento appena comparso non e' una discrepanza: raffiche e ritardi
    // di filtro ne producono in continuazione. Un allarme che grida sempre e' un
    // allarme che nessuno guarda piu'.
    auto diverging = reading(-200.0, efis::SignalId::vertical_speed, epoch + 1s);
    require(vertical.compare(baro, diverging, epoch + 1s).agreement ==
                efis::Agreement::agree,
            "a deviation shorter than the dwell must not be declared a disagreement");

    const auto sustained = vertical.compare(baro, diverging, epoch + 5s);
    require(sustained.agreement == efis::Agreement::disagree,
            "a deviation persisting beyond the dwell must be declared");
    require(sustained.difference == 700.0, "the observed deviation must be reported");
    require(sustained.exceeded_for >= 3000ms,
            "how long the deviation has lasted must be reported");

    // Rientrata la discrepanza, si torna in accordo e il conteggio riparte.
    require(vertical.compare(baro, satellite, epoch + 6s).agreement ==
                efis::Agreement::agree,
            "a deviation that returns within tolerance must clear");
    require(vertical.compare(baro, diverging, epoch + 7s).agreement ==
                efis::Agreement::agree,
            "after clearing, the dwell must be counted again from the start");

    // Con un termine di paragone inutilizzabile non c'e' confronto possibile, e
    // dichiarare accordo sarebbe una falsa rassicurazione.
    efis::CrossCheck guarded{{.tolerance = 10.0, .dwell = 1000ms}};
    auto lost = reading(0.0, efis::SignalId::ground_speed, epoch,
                        efis::Validity::unavailable);
    const auto no_comparison = guarded.compare(baro, lost, epoch);
    require(no_comparison.agreement == efis::Agreement::unknown,
            "without a usable second chain the result must be unknown, never agreement");

    // L'esito marca entrambi i campioni. Il confronto dice che le due catene non
    // concordano, non quale abbia torto: marcarne uno solo presenterebbe l'altro
    // come sano, che e' proprio l'informazione che non si possiede.
    efis::CrossCheck speeds{{.tolerance = 40.0, .dwell = 1000ms}};
    auto indicated = reading(82.0, efis::SignalId::indicated_airspeed, epoch);
    auto ground = reading(180.0, efis::SignalId::ground_speed, epoch);
    speeds.compare(indicated, ground, epoch);
    const auto declared = speeds.compare(indicated, ground, epoch + 2s);
    require(declared.agreement == efis::Agreement::disagree,
            "a gross speed mismatch sustained beyond the dwell must be declared");

    efis::apply_disagreement(declared, indicated, ground);
    require(indicated.validity == efis::Validity::disagree &&
                ground.validity == efis::Validity::disagree,
            "both chains must be marked: the check cannot tell which one is lying");
    require(!indicated.usable() && !ground.usable(),
            "a value in disagreement must not be presented as usable");

    // In accordo non si tocca nulla.
    auto healthy = reading(82.0, efis::SignalId::indicated_airspeed, epoch);
    auto companion = reading(85.0, efis::SignalId::ground_speed, epoch);
    efis::CrossCheck quiet{{.tolerance = 40.0, .dwell = 1000ms}};
    efis::apply_disagreement(quiet.compare(healthy, companion, epoch), healthy, companion);
    require(healthy.validity == efis::Validity::valid &&
                companion.validity == efis::Validity::valid,
            "agreement must leave both samples untouched");

    require(std::string{efis::agreement_name(efis::Agreement::disagree)} == "DISAGREE",
            "agreement names must stay stable for recorded data");


    // --- Collegamento allo stato di volo --------------------------------------
    //
    // I due meccanismi provati sopra in isolamento devono ora comparire nella
    // salute del sistema e negli allarmi, altrimenti restano codice disponibile
    // e non protezione.

    auto has_alert = [](const efis::FlightSnapshot& snapshot, const char* code) {
        for (const auto& alert : snapshot.alerts.active) {
            if (alert.code == code) return true;
        }
        return false;
    };
    auto alert_message = [](const efis::FlightSnapshot& snapshot, const char* code) {
        for (const auto& alert : snapshot.alerts.active) {
            if (alert.code == code) return alert.message;
        }
        return std::string{};
    };

    // Costruisce uno stato di volo in crociera, con tutti i campi utilizzabili e
    // freschi. I valori sono plausibili ma irrilevanti: conta che si muovano.
    auto cruising = [](efis::MonoTime at, double airspeed, double altitude,
                       double roll, double ground_speed) {
        efis::FlightSnapshot snapshot{};
        auto fill = [&](efis::Sample<double>& field, double value,
                        efis::SignalId signal) {
            field = reading(value, signal, at);
        };
        fill(snapshot.attitude.roll_deg, roll, efis::SignalId::roll);
        fill(snapshot.attitude.pitch_deg, 2.0 + roll, efis::SignalId::pitch);
        fill(snapshot.attitude.heading_deg, 42.0, efis::SignalId::heading);
        fill(snapshot.air_data.indicated_airspeed_kt, airspeed,
             efis::SignalId::indicated_airspeed);
        fill(snapshot.air_data.pressure_altitude_ft, altitude,
             efis::SignalId::pressure_altitude);
        fill(snapshot.air_data.vertical_speed_fpm, 0.0, efis::SignalId::vertical_speed);
        fill(snapshot.position.latitude_deg, 45.6, efis::SignalId::latitude);
        fill(snapshot.position.longitude_deg, 12.2, efis::SignalId::longitude);
        fill(snapshot.position.ground_speed_kt, ground_speed,
             efis::SignalId::ground_speed);
        fill(snapshot.guidance.commanded_roll_deg, 0.0, efis::SignalId::commanded_roll);
        fill(snapshot.guidance.commanded_pitch_deg, 0.0, efis::SignalId::commanded_pitch);
        fill(snapshot.guidance.selected_altitude_ft, 5500.0,
             efis::SignalId::selected_altitude);
        fill(snapshot.guidance.selected_course_deg, 42.0,
             efis::SignalId::selected_course);
        fill(snapshot.guidance.selected_airspeed_kt, 95.0,
             efis::SignalId::selected_airspeed);
        fill(snapshot.navigation.cdi_deviation_dots, 0.1, efis::SignalId::cdi_deviation);
        fill(snapshot.navigation.glide_slope_dots, 0.0,
             efis::SignalId::glide_slope_deviation);
        snapshot.guidance.flight_director_active =
            efis::Sample<bool>{true, efis::SourceId::pixhawk_primary, at, at,
                               efis::Quality::nominal, efis::Validity::valid, 1};
        snapshot.navigation.to_flag =
            efis::Sample<bool>{true, efis::SourceId::pixhawk_primary, at, at,
                               efis::Quality::nominal, efis::Validity::valid, 1};
        return snapshot;
    };

    // Volo sano: tutto si muove, nessuna delle due protezioni interviene.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot healthy_state{};
        for (int step = 0; step < 30; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            healthy_state = model.normalize(
                cruising(at, 95.0 + static_cast<double>(step) * 0.3,
                         5500.0 + static_cast<double>(step) * 4.0,
                         2.0 + static_cast<double>(step % 5) * 0.4,
                         100.0 + static_cast<double>(step) * 0.3),
                at);
        }
        require(healthy_state.health.suspect_fields == 0,
                "a healthy flight must not raise a frozen data warning");
        require(healthy_state.health.disagreeing_fields == 0,
                "a healthy flight must not raise a disagreement");
        require(healthy_state.health.level == efis::HealthLevel::normal,
                "a healthy flight must stay at normal health");
        require(!has_alert(healthy_state, "DATA_FROZEN"),
                "no frozen alert during a healthy flight");
    }

    // Pitot ostruito in crociera. La velocita' indicata si ferma, tutto il resto
    // continua a muoversi, e ogni campione resta fresco e valido. E' il guasto
    // che nessuno dei controlli preesistenti sarebbe in grado di vedere.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot blocked{};
        for (int step = 0; step < 30; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            const double airspeed = step < 5 ? 95.0 + static_cast<double>(step) * 0.3
                                             : 96.2;
            blocked = model.normalize(
                cruising(at, airspeed,
                         5500.0 + static_cast<double>(step) * 4.0,
                         2.0 + static_cast<double>(step % 5) * 0.4,
                         100.0 + static_cast<double>(step) * 0.3),
                at);
        }
        require(blocked.air_data.indicated_airspeed_kt.usable(),
                "the frozen airspeed still looks perfectly valid: that is the danger");
        require(blocked.health.suspect_fields >= 1,
                "a frozen airspeed must be counted in the system health");
        require(blocked.health.frozen_signal == efis::SignalId::indicated_airspeed,
                "the health must name which quantity has stopped moving");
        require(blocked.health.level == efis::HealthLevel::caution,
                "a frozen quantity must raise the health level");
        require(has_alert(blocked, "DATA_FROZEN"),
                "a frozen airspeed must reach the alert list");
        require(alert_message(blocked, "DATA_FROZEN") == "indicated_airspeed FROZEN",
                "the alert must name the quantity, not just say that one is frozen");
    }

    // A terra fermi non si grida. Assetto, quota e velocita' sono legittimamente
    // costanti a motore acceso, e un allarme a ogni sosta e' un allarme che
    // nessuno guarda piu'.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot parked{};
        for (int step = 0; step < 60; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            parked = model.normalize(cruising(at, 0.0, 59.0, 0.0, 0.0), at);
        }
        require(parked.health.suspect_fields == 0,
                "a stationary aircraft must not be reported as frozen");
        require(!has_alert(parked, "DATA_FROZEN"),
                "no frozen alert while parked, however long the wait");
    }

    // Il caso che giustifica il cancello sulla velocita' al suolo: durante la
    // corsa di decollo la sorveglianza si apre, e un pitot gia' ostruito viene
    // colto proprio quando comincia a contare.
    //
    // Le due protezioni intervengono in momenti diversi. La stasi matura per
    // prima, quando la velocita' indicata e' ferma da abbastanza tempo; poi il
    // divario con la velocita' al suolo supera ogni vento plausibile e scatta il
    // confronto, che marca la velocita' indicata come inutilizzabile. Da quel
    // momento la stasi non la sorveglia piu', perche' non sorveglia i dati gia'
    // dichiarati inservibili. La copertura non si interrompe: cambia soltanto
    // quale meccanismo la fornisce, e la segnalazione al pilota si aggrava da
    // avvertimento a allarme.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot rolling{};
        bool airspeed_named_frozen = false;
        bool airspeed_flagged_throughout = true;
        bool roll_started = false;
        for (int step = 0; step < 40; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            const double ground_speed = static_cast<double>(step) * 3.0;
            rolling = model.normalize(
                cruising(at, 12.0, 59.0 + static_cast<double>(step) * 2.0,
                         0.2 * static_cast<double>(step % 3), ground_speed),
                at);
            if (rolling.health.frozen_signal == efis::SignalId::indicated_airspeed) {
                airspeed_named_frozen = true;
                roll_started = true;
            }
            if (rolling.health.disagreeing_fields > 0) roll_started = true;
            // Una volta cominciata la segnalazione non deve piu' interrompersi:
            // un buco fra la fine di una protezione e l'inizio dell'altra
            // lascerebbe il pilota senza avviso proprio durante il decollo.
            if (roll_started && rolling.health.suspect_fields == 0 &&
                rolling.health.disagreeing_fields == 0) {
                airspeed_flagged_throughout = false;
            }
        }
        require(airspeed_named_frozen,
                "a pitot already blocked must be named once the roll begins");
        require(airspeed_flagged_throughout,
                "the warning must not lapse when one protection hands over to the other");
        require(rolling.health.disagreeing_fields == 2,
                "by the end of the roll the speed disagreement must be declared");
        require(rolling.health.level == efis::HealthLevel::warning,
                "the blocked pitot must end the roll as a warning, not a caution");
    }

    // Confronto incrociato: velocita' indicata ferma mentre quella al suolo
    // cresce oltre ogni vento plausibile. Entrambe vengono marcate, perche' il
    // confronto non dice quale delle due menta.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot diverged{};
        for (int step = 0; step < 20; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            diverged = model.normalize(
                cruising(at, 60.0, 5500.0 + static_cast<double>(step) * 4.0,
                         2.0 + static_cast<double>(step % 5) * 0.4,
                         60.0 + static_cast<double>(step) * 8.0),
                at);
        }
        require(diverged.health.disagreeing_fields == 2,
                "a disagreement must mark both chains, not one");
        require(diverged.air_data.indicated_airspeed_kt.validity ==
                    efis::Validity::disagree,
                "the indicated airspeed must be marked in disagreement");
        require(diverged.position.ground_speed_kt.validity == efis::Validity::disagree,
                "the ground speed must be marked in disagreement too");
        require(!diverged.air_data.indicated_airspeed_kt.usable(),
                "a value in disagreement must not be presented as usable");
        require(diverged.health.level == efis::HealthLevel::warning,
                "a disagreement must raise the health level to warning");
        require(has_alert(diverged, "DATA_DISAGREE"),
                "the disagreement must reach the alert list");
    }

    // Una protezione non deve disattivarne un'altra. Il confronto marca la
    // velocita' al suolo come inutilizzabile: se il moto fosse valutato dopo, il
    // cancello si chiuderebbe e la sorveglianza della stasi si spegnerebbe in
    // silenzio proprio mentre e' in corso un guasto.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot both{};
        for (int step = 0; step < 30; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            both = model.normalize(
                cruising(at, 60.0, 5500.0,
                         2.0 + static_cast<double>(step % 5) * 0.4,
                         60.0 + static_cast<double>(step) * 8.0),
                at);
        }
        require(both.health.disagreeing_fields == 2,
                "the disagreement must still be declared");
        require(both.health.suspect_fields >= 1,
                "the frozen altitude must still be caught while a disagreement runs");
    }

    // La sorveglianza riparte da capo dopo una sosta: il valore costante
    // osservato a terra non deve contare come immobilita' al decollo successivo.
    {
        efis::FlightModel model{};
        for (int step = 0; step < 40; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            // Lo snapshot prodotto qui non interessa: conta solo che la
            // sorveglianza veda passare il tempo con valori fermi. Lo scarto va
            // dichiarato, altrimenti nodiscard lo segnala e il progetto smette
            // di compilare senza avvisi.
            static_cast<void>(model.normalize(cruising(at, 0.0, 59.0, 0.0, 0.0), at));
        }
        const auto after_stop = epoch + 41s;
        const auto first_moving = model.normalize(
            cruising(after_stop, 40.0, 59.0, 0.0, 45.0), after_stop);
        require(first_moving.health.suspect_fields == 0,
                "the watch must restart after a stop, not inherit the standstill");
    }


    // --- Presa statica ostruita ------------------------------------------------
    //
    // Il confronto fra velocita' verticale barometrica e satellitare intercetta
    // il guasto che la sola sorveglianza della stasi non vede: la presa statica
    // si ostruisce, il barometro smette di seguire la quota, e il satellitare
    // continua a vederla cambiare.
    //
    // Si confrontano le velocita' e non le quote. Quella barometrica e quella
    // satellitare differiscono sempre, per regolazione barometrica e separazione
    // del geoide: uno scarto costante di centinaia di piedi e' normale, e un
    // confronto sui valori assoluti direbbe "disaccordo" a ogni volo.

    // Costruisce uno stato con le due velocita' verticali indipendenti.
    auto climbing = [&cruising](efis::MonoTime at, double baro_fpm, double gnss_fpm,
                                double altitude, double ground_speed) {
        auto snapshot = cruising(at, 95.0, altitude, 2.0, ground_speed);
        snapshot.air_data.vertical_speed_fpm =
            reading(baro_fpm, efis::SignalId::vertical_speed, at);
        snapshot.position.gnss_vertical_speed_fpm =
            reading(gnss_fpm, efis::SignalId::gnss_vertical_speed, at);
        return snapshot;
    };

    // Salita sana: le due catene concordano entro la tolleranza.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot climb{};
        for (int step = 0; step < 20; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            climb = model.normalize(
                climbing(at, 700.0 + static_cast<double>(step % 4) * 20.0,
                         680.0 + static_cast<double>(step % 3) * 25.0,
                         2000.0 + static_cast<double>(step) * 12.0, 100.0),
                at);
        }
        require(climb.health.disagreeing_fields == 0,
                "two vertical chains that agree must not be reported as disagreeing");
        require(climb.air_data.vertical_speed_fpm.usable(),
                "an agreeing barometric vertical speed stays usable");
        require(model.vertical_agreement().agreement == efis::Agreement::agree,
                "the vertical cross check must report agreement");
    }

    // Presa statica ostruita: il barometro si ferma su zero mentre il satellitare
    // continua a dichiarare una salita. Nessun vento puo' spiegarlo, perche' qui
    // il vento non c'entra: le due grandezze descrivono lo stesso moto.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot blocked{};
        for (int step = 0; step < 20; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            blocked = model.normalize(
                climbing(at, 0.0, 800.0,
                         2000.0 + static_cast<double>(step) * 12.0, 100.0),
                at);
        }
        require(blocked.health.disagreeing_fields >= 2,
                "a blocked static port must be caught by the vertical cross check");
        require(blocked.air_data.vertical_speed_fpm.validity == efis::Validity::disagree,
                "the barometric vertical speed must be marked in disagreement");
        require(blocked.position.gnss_vertical_speed_fpm.validity ==
                    efis::Validity::disagree,
                "the satellite vertical speed must be marked too: neither is trusted");
        require(blocked.health.level == efis::HealthLevel::warning,
                "a vertical disagreement must raise the health level to warning");
    }

    // Una raffica verticale produce uno scarto istantaneo del tutto normale. Se
    // bastasse quello, lo strumento griderebbe a ogni giornata turbolenta.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot gusty{};
        for (int step = 0; step < 20; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            // Uno scarto oltre la tolleranza per un solo campione ogni cinque,
            // troppo breve per superare la persistenza richiesta.
            const double gnss = step % 5 == 0 ? 1400.0 : 520.0;
            gusty = model.normalize(
                climbing(at, 500.0, gnss,
                         2000.0 + static_cast<double>(step) * 8.0, 100.0),
                at);
        }
        require(gusty.health.disagreeing_fields == 0,
                "a transient gust must not be declared a disagreement");
    }

    // Senza fix tridimensionale la velocita' verticale satellitare non e' un
    // termine di paragone, e il confronto deve tacere invece di dichiarare un
    // disaccordo che dipende dal guasto gia' noto.
    {
        efis::FlightModel model{};
        efis::FlightSnapshot without_fix{};
        for (int step = 0; step < 20; ++step) {
            const auto at = epoch + std::chrono::seconds{step};
            auto snapshot = climbing(at, 0.0, 800.0,
                                     2000.0 + static_cast<double>(step) * 12.0, 100.0);
            snapshot.position.gnss_vertical_speed_fpm.validity =
                efis::Validity::unavailable;
            without_fix = model.normalize(snapshot, at);
        }
        require(model.vertical_agreement().agreement == efis::Agreement::unknown,
                "without a usable satellite chain the comparison must stay unknown");
        require(without_fix.air_data.vertical_speed_fpm.validity !=
                    efis::Validity::disagree,
                "a missing comparison must not be turned into a disagreement");
    }

    require(std::string{efis::signal_name(efis::SignalId::gnss_vertical_speed)} ==
                "gnss_vertical_speed",
            "the new signal name must stay stable for recorded data");

    std::cout << "All EFIS safety tests passed\n";
}
