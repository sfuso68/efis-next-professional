// Audit indipendente EMS R1-30 — comportamento del nucleo motore senza Qt.
//
// Questo file appartiene alla review `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md`
// e non e' dichiarato in CMake: l'incarico vieta di modificare CMake/CI. Si
// compila ed esegue a mano con il comando riportato nella review.
//
// Il test esercita soltanto le unita' del nucleo che non dipendono da Qt
// (`RuntimeTracker`, `engine_integration_input`, `present_ems_link`) e non
// modifica alcun file di runtime, schema, registro o output generato.
//
// Ogni caso stampa `PASS` o `FAIL` e il processo esce con 0 se e solo se tutti
// i casi sono PASS. I casi marcati GAP descrivono un comportamento osservato
// che la review considera un difetto: sono verdi oggi e devono essere
// aggiornati quando il difetto viene chiuso.

#include "efis/core/engine_runtime.hpp"
#include "efis/core/derived_state.hpp"
#include "efis/display/ems_link_presentation.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, std::string_view name) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s  %.*s\n", condition ? "PASS" : "FAIL",
                static_cast<int>(name.size()), name.data());
}

efis::Sample<double> sample(double value, efis::Validity validity,
                            bool simulated = false) {
    efis::Sample<double> out{};
    out.value = value;
    out.validity = validity;
    out.quality = efis::Quality::nominal;
    out.origin.simulated = simulated;
    out.raw = value;
    return out;
}

// --- Ore motore ------------------------------------------------------------

// Il profilo attivo dichiara `tach_reference_rpm`. Il costruttore del nodo EMS
// applica quel valore solo quando non esiste uno stato salvato: sul ramo
// RECOVERED ricostruisce il tracker con due letterali fissi.
// Vedi src/nodes/ems/ems_controller.cpp:77 contro :334-340.
void runtime_profile_reference_is_discarded_on_recovery() {
    const double seconds = 3600.0;
    const double rpm = 5000.0;

    // Ramo profilo: riferimento tach dichiarato dal profilo del motore.
    efis::engine::RuntimeTracker from_profile{{500.0, 5500.0}, {}};
    from_profile.update(rpm, seconds, false);

    // Ramo RECOVERED: riferimento ricostruito dai letterali del costruttore.
    efis::engine::RuntimeTracker from_literals{{500.0, 5000.0}, {}};
    from_literals.update(rpm, seconds, false);

    const double profile_tach = from_profile.totals().tach_hours;
    const double literal_tach = from_literals.totals().tach_hours;

    check(std::abs(profile_tach - 5000.0 / 5500.0) < 1e-12,
          "tach-hours follow the declared tach reference when the profile is applied");
    check(std::abs(literal_tach - 1.0) < 1e-12,
          "tach-hours follow the hard-coded 5000 reference on the recovery branch");
    check(std::abs(literal_tach - profile_tach) > 0.09,
          "GAP: the two branches disagree by more than 9% of an hour per hour run");
    // Le ore motore vere coincidono: solo il tach-time divergerebbe.
    check(std::abs(from_profile.totals().engine_hours -
                   from_literals.totals().engine_hours) < 1e-12,
          "engine-run hours are unaffected by the tach reference");
}

// `RuntimeTracker::update` incrementa `flight_hours` solo se `airborne` e' vero.
// Il nodo EMS passa sempre `false` (src/nodes/ems/ems_controller.cpp:783).
void flight_hours_cannot_accumulate_without_an_airborne_input() {
    efis::engine::RuntimeTracker tracker{{500.0, 5000.0}, {}};
    for (int hour = 0; hour < 10; ++hour) tracker.update(5000.0, 3600.0, false);
    check(tracker.totals().flight_hours == 0.0,
          "GAP: flight hours stay at zero after ten hours at cruise rpm");
    check(std::abs(tracker.totals().engine_hours - 10.0) < 1e-9,
          "engine-run hours do accumulate over the same ten hours");
    efis::engine::RuntimeTracker airborne{{500.0, 5000.0}, {}};
    airborne.update(5000.0, 3600.0, true);
    check(std::abs(airborne.totals().flight_hours - 1.0) < 1e-12,
          "the counter itself works when an airborne input is supplied");
}

// --- Soglia di "motore in moto" -------------------------------------------

// Due soglie indipendenti descrivono la stessa condizione fisica: 400 rpm in
// src/core/derived_state.cpp:18 e `engine_run_threshold_rpm` (500 nel profilo
// attivo) in RuntimeTracker. Fra le due esiste una finestra in cui il
// carburante viene integrato mentre le ore motore non avanzano.
void fuel_integrates_in_the_window_where_the_engine_is_not_running() {
    const double seconds = 1.0;
    const auto rpm = sample(450.0, efis::Validity::valid);
    const auto flow = sample(4.8, efis::Validity::valid);

    const auto input = efis::engine_integration_input(rpm, flow, seconds);
    check(input.fuel_usable,
          "fuel is integrated at 450 rpm because the local threshold is 400");
    check(input.fuel_consumed_gallons > 0.0,
          "a non-zero quantity of fuel is charged to the tanks");

    efis::engine::RuntimeTracker tracker{{500.0, 5000.0}, {}};
    tracker.update(input.accountable_rpm, seconds, false);
    check(!tracker.engine_running(),
          "the runtime tracker reports the engine as not running at the same instant");
    check(tracker.totals().engine_hours == 0.0,
          "GAP: fuel burn is recorded while engine-run hours are withheld");
}

// --- Validita' per canale --------------------------------------------------

// `is_usable` ammette `simulated`, quindi l'integratore accetta un flusso
// simulato. Il nodo separa poi i due accumulatori, ma la decisione di
// integrare non distingue il dato simulato da quello reale.
void simulated_fuel_flow_is_accepted_by_the_integrator() {
    const auto rpm = sample(5000.0, efis::Validity::simulated, true);
    const auto flow = sample(4.8, efis::Validity::simulated, true);
    const auto input = efis::engine_integration_input(rpm, flow, 1.0);
    check(input.simulated, "the integration input is marked simulated");
    check(input.fuel_usable,
          "GAP: a simulated fuel flow still satisfies the usability gate");
    check(!input.runtime_usable,
          "engine hours correctly refuse a simulated rpm");
    check(input.accountable_rpm == 0.0,
          "the accountable rpm is zeroed for a simulated sample");
}

// Un canale non disponibile azzera l'rpm contabilizzabile e blocca il
// carburante: il nucleo e' corretto, e il difetto sta a valle, nella
// superficie Qt che pubblica solo `.value`.
void an_unavailable_channel_blocks_both_integrators() {
    for (const auto validity : {efis::Validity::unavailable,
                                efis::Validity::stale,
                                efis::Validity::out_of_range,
                                efis::Validity::disagree}) {
        const auto rpm = sample(5000.0, validity);
        const auto flow = sample(4.8, efis::Validity::valid);
        const auto input = efis::engine_integration_input(rpm, flow, 1.0);
        check(!input.runtime_usable && input.accountable_rpm == 0.0,
              "an unusable rpm never reaches the hour meter");
        check(!input.fuel_usable,
              "an unusable rpm also stops the fuel integration");
    }
}

// --- Presentazione del collegamento EDP ------------------------------------

// ADR-011 impone la dicitura `SIM FALLBACK • LINK LOST`. La politica la
// produce in `engine_state`, mentre `data_state` resta `LOST`.
void sim_fallback_wording_lives_only_in_the_engine_state_field() {
    using efis::protocol::edp::LinkState;
    const auto lost = efis::display::present_ems_link(LinkState::lost, false);
    const auto waiting = efis::display::present_ems_link(LinkState::waiting, false);

    check(lost.engine_state == std::string_view{"SIM FALLBACK • LINK LOST"},
          "the lost link carries the ADR-011 sim-fallback wording");
    check(waiting.engine_state == std::string_view{"SIM FALLBACK • WAITING"},
          "the waiting link carries the sim-fallback wording as well");
    check(lost.data_state == std::string_view{"LOST"} &&
              waiting.data_state == std::string_view{"LOST"},
          "GAP: the field the EMS gauges consume says only LOST");
    check(!lost.use_network && !waiting.use_network,
          "both states hand the display back to the simulation provider");

    const auto live_sim = efis::display::present_ems_link(LinkState::live, true);
    check(live_sim.data_state == std::string_view{"SIM"},
          "a live but simulated publisher is marked SIM on the gauges");
    const auto live = efis::display::present_ems_link(LinkState::live, false);
    check(live.data_state == std::string_view{"LIVE"},
          "a live real publisher is marked LIVE");
}

}  // namespace

int main() {
    std::printf("== EMS core behaviour, audit R1-30 ==\n");
    runtime_profile_reference_is_discarded_on_recovery();
    flight_hours_cannot_accumulate_without_an_airborne_input();
    fuel_integrates_in_the_window_where_the_engine_is_not_running();
    simulated_fuel_flow_is_accepted_by_the_integrator();
    an_unavailable_channel_blocks_both_integrators();
    sim_fallback_wording_lives_only_in_the_engine_state_field();
    std::printf("== %d checks, %d failures ==\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
