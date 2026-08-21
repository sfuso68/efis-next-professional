// Prove delle protezioni contro il dato plausibile ma falso.
//
// Sono separate dalle prove del nucleo perche' verificano una categoria diversa:
// non che il sistema calcoli il valore giusto, ma che si accorga quando il
// valore che riceve non significa piu' nulla. Un guasto qui non produce un
// numero sbagliato di poco, produce un numero fermo e convincente.

#include "efis/core/cross_check.hpp"
#include "efis/core/stasis_monitor.hpp"
#include "efis/core/flight_dynamics.hpp"
#include "efis/core/flight_model.hpp"
#include "efis/providers/mavlink_adapter.hpp"

#include <chrono>
#include <cmath>
#include <vector>
#include <span>
#include <cstring>
#include <array>
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
        // Il rateo di virata segue dall'inclinazione e dalla velocita' di questo
        // stesso istantaneo: un volo di prova incoerente con se stesso non
        // proverebbe granche'.
        fill(snapshot.attitude.turn_rate_deg_s,
             efis::coordinated_turn_rate_deg_s(roll, airspeed), efis::SignalId::turn_rate);
        fill(snapshot.attitude.lateral_acceleration_g, 0.0,
             efis::SignalId::lateral_acceleration);
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


    // --- Autodiagnosi dell'autopilota -----------------------------------------
    //
    // ArduPilot confronta gia' fra loro le proprie sorgenti e pubblica il
    // risultato. E' il confronto incrociato piu' economico che abbiamo, e finora
    // veniva decodificato e buttato.

    // Costruttore di frame MAVLink 2. Il checksum e' quello che impone il
    // protocollo, non uno scelto qui: se il CRC_EXTRA della tabella fosse
    // sbagliato questi frame verrebbero rifiutati e la prova fallirebbe.
    auto crc_of = [](std::vector<std::uint8_t> body, std::uint8_t extra) {
        std::uint16_t crc = 0xFFFF;
        body.push_back(extra);
        for (std::uint8_t byte : body) {
            auto tmp = static_cast<std::uint8_t>(byte ^ static_cast<std::uint8_t>(crc & 0xFF));
            tmp = static_cast<std::uint8_t>(tmp ^ static_cast<std::uint8_t>(tmp << 4));
            crc = static_cast<std::uint16_t>((crc >> 8) ^ (static_cast<std::uint16_t>(tmp) << 8) ^
                                             (static_cast<std::uint16_t>(tmp) << 3) ^
                                             (static_cast<std::uint16_t>(tmp) >> 4));
        }
        return crc;
    };
    auto frame_of = [&crc_of](std::uint32_t id, const std::vector<std::uint8_t>& payload) {
        const auto* definition = efis::mavlink::find_message(id);
        require(definition != nullptr, "the message must be in the generated table");
        std::vector<std::uint8_t> body{
            static_cast<std::uint8_t>(payload.size()), 0, 0, 1, 1, 1,
            static_cast<std::uint8_t>(id & 0xFF),
            static_cast<std::uint8_t>((id >> 8) & 0xFF),
            static_cast<std::uint8_t>((id >> 16) & 0xFF)};
        body.insert(body.end(), payload.begin(), payload.end());
        const std::uint16_t crc = crc_of(body, definition->crc_extra);
        std::vector<std::uint8_t> frame{0xFD};
        frame.insert(frame.end(), body.begin(), body.end());
        frame.push_back(static_cast<std::uint8_t>(crc & 0xFF));
        frame.push_back(static_cast<std::uint8_t>(crc >> 8));
        return frame;
    };
    auto little_endian_u32 = [](std::uint32_t value) {
        return std::vector<std::uint8_t>{
            static_cast<std::uint8_t>(value & 0xFF),
            static_cast<std::uint8_t>((value >> 8) & 0xFF),
            static_cast<std::uint8_t>((value >> 16) & 0xFF),
            static_cast<std::uint8_t>((value >> 24) & 0xFF)};
    };

    // EKF_STATUS_REPORT sul filo: cinque float di varianza, poi i flag a 16 bit,
    // poi la varianza sulla velocita' vera dell'aria, che e' un campo di
    // estensione. Un trasmettitore che non la conosce si ferma prima, e in quel
    // caso il valore e' assente, non nullo.
    auto ekf_frame = [&](std::uint16_t flags, float airspeed_variance,
                         bool with_extension = true) {
        auto append_float = [&](std::vector<std::uint8_t>& into, float value) {
            std::uint32_t bits{};
            std::memcpy(&bits, &value, sizeof(bits));
            const auto encoded = little_endian_u32(bits);
            into.insert(into.end(), encoded.begin(), encoded.end());
        };
        std::vector<std::uint8_t> payload;
        for (const float value : {0.2F, 0.3F, 0.1F, 0.05F, 0.02F}) {
            append_float(payload, value);
        }
        payload.push_back(static_cast<std::uint8_t>(flags & 0xFF));
        payload.push_back(static_cast<std::uint8_t>((flags >> 8) & 0xFF));
        if (with_extension) append_float(payload, airspeed_variance);
        return frame_of(193, payload);
    };

    constexpr std::uint16_t ekf_attitude = 1;
    constexpr std::uint16_t ekf_velocity_horizontal = 2;
    constexpr std::uint16_t ekf_velocity_vertical = 4;
    constexpr std::uint16_t ekf_position_horizontal = 16;
    constexpr std::uint16_t ekf_constant_position = 128;
    constexpr std::uint16_t ekf_uninitialised = 1024;
    constexpr std::uint16_t ekf_healthy = ekf_attitude | ekf_velocity_horizontal |
                                          ekf_velocity_vertical | ekf_position_horizontal;

    // Filtro sano: i bit vengono letti e nulla viene degradato.
    {
        efis::MavlinkAdapter adapter;
        const auto healthy = ekf_frame(ekf_healthy, 0.4F);
        adapter.consume(std::span<const std::uint8_t>{healthy.data(), healthy.size()}, epoch);
        const auto& estimator = adapter.estimator();
        require(estimator.reported, "the autopilot self report must be recorded");
        require(estimator.attitude_healthy, "a set attitude bit means a healthy estimate");
        require(estimator.vertical_velocity_healthy,
                "a set vertical velocity bit means a healthy estimate");
        require(!estimator.uninitialised, "a healthy filter is not uninitialised");
        require(!estimator.dead_reckoning, "a healthy filter is not dead reckoning");
        require(std::abs(estimator.airspeed_variance - 0.4) < 1e-5,
                "the airspeed variance is the autopilot judgement on the air sensor");
        require(std::abs(estimator.velocity_variance - 0.2) < 1e-5,
                "the recorded variances must decode from their own offsets");
    }

    // Trasmettitore che non conosce il campo di estensione: la varianza sulla
    // velocita' dell'aria resta quella di prima invece di essere azzerata. Un
    // campo non trasmesso e' assente, e sovrascriverlo con zero significherebbe
    // dichiarare che l'autopilota giudica perfetto un sensore su cui non si e'
    // pronunciato.
    {
        efis::MavlinkAdapter adapter;
        const auto complete = ekf_frame(ekf_healthy, 0.7F);
        adapter.consume(std::span<const std::uint8_t>{complete.data(), complete.size()},
                        epoch);
        require(std::abs(adapter.estimator().airspeed_variance - 0.7) < 1e-5,
                "the extension must decode when transmitted");

        const auto truncated = ekf_frame(ekf_healthy, 0.0F, false);
        adapter.consume(std::span<const std::uint8_t>{truncated.data(), truncated.size()},
                        epoch + 1s);
        require(std::abs(adapter.estimator().airspeed_variance - 0.7) < 1e-5,
                "an untransmitted extension must not overwrite what was known");
    }

    // Un adapter che non ha mai ricevuto il rapporto non e' sano: non si sa.
    {
        efis::MavlinkAdapter silent;
        require(!silent.estimator().reported,
                "never having heard is not the same as being healthy");
    }

    // Assetto dichiarato non sano: il valore resta mostrato e viene marcato
    // degradato. Toglierlo al pilota nel momento in cui gli serve sarebbe peggio
    // che mostrarglielo dichiarando che non e' affidabile.
    {
        efis::MavlinkAdapter adapter;
        const auto attitude = frame_of(30, std::vector<std::uint8_t>(28, 0));
        adapter.consume(std::span<const std::uint8_t>{attitude.data(), attitude.size()}, epoch);
        require(adapter.snapshot().attitude.roll_deg.validity == efis::Validity::valid,
                "without a report the attitude keeps the validity it had");

        const auto degraded = ekf_frame(
            static_cast<std::uint16_t>(ekf_healthy & ~ekf_attitude), 0.4F);
        adapter.consume(std::span<const std::uint8_t>{degraded.data(), degraded.size()},
                        epoch + 1s);
        require(adapter.snapshot().attitude.roll_deg.validity == efis::Validity::degraded,
                "an attitude the autopilot calls unhealthy must be marked degraded");
        require(adapter.snapshot().attitude.roll_deg.usable(),
                "degraded is still shown: it is not the same as absent");
    }

    // Filtro non inizializzato: qui una stima non esiste affatto, e mostrarla
    // sarebbe mostrare un numero senza contenuto.
    {
        efis::MavlinkAdapter adapter;
        const auto attitude = frame_of(30, std::vector<std::uint8_t>(28, 0));
        adapter.consume(std::span<const std::uint8_t>{attitude.data(), attitude.size()}, epoch);
        const auto blank = ekf_frame(ekf_uninitialised, 0.0F);
        adapter.consume(std::span<const std::uint8_t>{blank.data(), blank.size()}, epoch + 1s);
        require(adapter.estimator().uninitialised, "the uninitialised bit must be read");
        // Degradato e non assente. I bit descrivono lo stato dell'EKF, non quello
        // del messaggio di assetto, che ArduPilot pubblica comunque ricavandolo
        // altrimenti: dedurne che l'assetto non esista sarebbe un'affermazione
        // sugli interni dell'autopilota che qui non e' verificata.
        require(adapter.snapshot().attitude.roll_deg.validity == efis::Validity::degraded,
                "an uninitialised filter makes the attitude uncertain, not absent");
        require(adapter.snapshot().attitude.roll_deg.usable(),
                "an uncertain attitude is still shown to the pilot");
    }

    // Navigazione a stima: il fix resta dichiarato valido mentre la posizione
    // non descrive piu' dove si trova l'aeromobile. Il bit e' l'unica notizia
    // che lo rivela.
    {
        efis::MavlinkAdapter adapter;
        std::vector<std::uint8_t> gps(30, 0);
        // Offset presi dalla tabella generata, non indovinati: in GPS_RAW_INT
        // l'ordine di filo mette i campi grandi per primi, e fix_type finisce
        // in fondo alla parte base.
        gps[28] = 3;   // fix_type tridimensionale
        gps[29] = 12;  // satelliti visibili
        const auto fix = frame_of(24, gps);
        adapter.consume(std::span<const std::uint8_t>{fix.data(), fix.size()}, epoch);
        require(adapter.snapshot().position.latitude_deg.validity == efis::Validity::valid,
                "a three dimensional fix makes the position valid");

        const auto reckoning = ekf_frame(
            static_cast<std::uint16_t>(ekf_healthy | ekf_constant_position), 0.4F);
        adapter.consume(std::span<const std::uint8_t>{reckoning.data(), reckoning.size()},
                        epoch + 1s);
        require(adapter.estimator().dead_reckoning, "the constant position bit must be read");
        require(adapter.snapshot().position.latitude_deg.validity ==
                    efis::Validity::degraded,
                "a position held by dead reckoning must not stay plainly valid");
    }

    // Vibrazioni: conta la crescita dei contatori di saturazione, non il fatto
    // che in passato l'aeromobile abbia vibrato.
    {
        efis::MavlinkAdapter adapter;
        auto vibration_frame = [&](std::uint32_t clipping) {
            std::vector<std::uint8_t> payload(8, 0);  // time_usec
            for (int axis = 0; axis < 3; ++axis) {
                const float value = 12.5F;
                std::uint32_t bits{};
                std::memcpy(&bits, &value, sizeof(bits));
                const auto encoded = little_endian_u32(bits);
                payload.insert(payload.end(), encoded.begin(), encoded.end());
            }
            for (int counter = 0; counter < 3; ++counter) {
                const auto encoded = little_endian_u32(clipping);
                payload.insert(payload.end(), encoded.begin(), encoded.end());
            }
            return frame_of(241, payload);
        };

        const auto first = vibration_frame(4);
        adapter.consume(std::span<const std::uint8_t>{first.data(), first.size()}, epoch);
        require(adapter.vibration().reported, "the vibration report must be recorded");
        require(!adapter.vibration().clipping_increasing,
                "the first report establishes the baseline, it is not growth");
        require(std::abs(adapter.vibration().z_axis - 12.5) < 1e-5,
                "the vibration level must decode");

        const auto same = vibration_frame(4);
        adapter.consume(std::span<const std::uint8_t>{same.data(), same.size()}, epoch + 1s);
        require(!adapter.vibration().clipping_increasing,
                "a counter that stays put is past saturation, not present saturation");

        const auto grown = vibration_frame(9);
        adapter.consume(std::span<const std::uint8_t>{grown.data(), grown.size()}, epoch + 2s);
        require(adapter.vibration().clipping_increasing,
                "a growing counter means the accelerometer is saturating now");
        require(adapter.vibration().clipping_0 == 9, "the counter value must be kept");
    }

    // Un campo degradato viene contato nella salute e detto negli allarmi: un
    // valore che si vede e non si sa quanto valga e' peggio di uno che si vede
    // sapendo che e' incerto.
    {
        efis::FlightModel model{};
        auto snapshot = cruising(epoch, 95.0, 5500.0, 2.0, 100.0);
        snapshot.attitude.roll_deg.validity = efis::Validity::degraded;
        const auto normalised = model.normalize(snapshot, epoch);
        require(normalised.health.degraded_fields == 1,
                "a degraded field must be counted in the system health");
        require(has_alert(normalised, "DATA_DEGRADED"),
                "a degraded field must reach the alert list");
    }

    std::cout << "All EFIS safety tests passed\n";
}
