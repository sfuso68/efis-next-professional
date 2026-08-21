// Prove delle protezioni contro il dato plausibile ma falso.
//
// Sono separate dalle prove del nucleo perche' verificano una categoria diversa:
// non che il sistema calcoli il valore giusto, ma che si accorga quando il
// valore che riceve non significa piu' nulla. Un guasto qui non produce un
// numero sbagliato di poco, produce un numero fermo e convincente.

#include "efis/core/cross_check.hpp"
#include "efis/core/stasis_monitor.hpp"

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

    std::cout << "All EFIS safety tests passed\n";
}
