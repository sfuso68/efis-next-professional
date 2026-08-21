#include "efis/instruments/horizon.hpp"
#include "efis/core/flight_dynamics.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace efis::instruments {
namespace {

// Quanto raggio vale un grado di beccheggio. Con questo fattore l'area visibile
// copre circa trenta gradi sopra e sotto l'orizzonte, che e' la compressione
// abituale: piu' fitta rende illeggibili gli assetti normali, piu' larga fa
// uscire l'orizzonte dallo strumento in salita ripida.
constexpr double radius_per_pitch_degree = 0.025;

// La scala di beccheggio ha un gradino ogni cinque gradi, lungo ogni dieci.
constexpr double rung_step_deg = 5.0;
constexpr double major_rung_half_width = 0.30;
constexpr double minor_rung_half_width = 0.15;
constexpr double ladder_limit_deg = 30.0;

// Oltre questi valori non si tratta piu' di assetto ma di dato corrotto:
// nessun aeromobile ha un beccheggio oltre la verticale, e il rollio si
// avvolge invece di superarli.
constexpr double pitch_limit_deg = 90.0;

// Tacche della scala di inclinazione, ai valori su cui si vola davvero.
constexpr double bank_marks_deg[] = {-60.0, -45.0, -30.0, -20.0, -10.0, 0.0,
                                     10.0, 20.0, 30.0, 45.0, 60.0};

// Rateo oltre il quale la sagoma non si inclina di piu'. Il quadrante e' tarato
// sulla virata standard, e mostrare un rateo tripla facendo ruotare la sagoma
// oltre la verticale la renderebbe illeggibile.
constexpr double turn_display_limit_deg_s = 6.0;
constexpr double aircraft_bank_at_limit_deg = 30.0;

// Accelerazione laterale che porta la pallina a fondo tubo. Oltre un quinto di
// g la scivolata e' gia' grossolana, e un tubo tarato piu' largo renderebbe
// invisibili le scoordinazioni piccole, che sono quelle che si correggono.
constexpr double slip_full_scale_g = 0.2;

double normalise_roll(double roll_deg) noexcept {
    double value = std::fmod(roll_deg + 180.0, 360.0);
    if (value < 0.0) value += 360.0;
    return value - 180.0;
}

}  // namespace

Horizon describe_attitude(const AttitudeReading& reading, const Sizing& sizing) {
    Horizon horizon;
    horizon.simulated = reading.roll.simulated || reading.pitch.simulated;

    // La scala di inclinazione appartiene alla ghiera e non al quadro: e'
    // rispetto a lei che si legge il rollio, quindi resta anche senza assetto.
    for (const double mark : bank_marks_deg) {
        const bool major = std::abs(mark) == 0.0 || std::abs(mark) == 30.0 ||
                           std::abs(mark) == 60.0;
        horizon.bank_scale.push_back(
            Tick{mark, major ? 0.86 : 0.91, 0.98,
                 major ? TickKind::major : TickKind::minor});
    }

    // Rollio e beccheggio si guastano insieme, perche' vengono dalla stessa
    // piattaforma: basta che uno manchi perche' l'assetto non sia mostrabile.
    if (!is_usable(reading.roll.validity) || !is_usable(reading.pitch.validity)) {
        const bool stale = reading.roll.validity == Validity::stale ||
                           reading.pitch.validity == Validity::stale;
        horizon.annunciation = stale ? Annunciation::stale : Annunciation::failed;
        return horizon;
    }

    const double pitch = reading.pitch.value;
    if (std::abs(pitch) > pitch_limit_deg) {
        // Nessun aeromobile ha un beccheggio oltre la verticale. Disegnarlo
        // produrrebbe un quadro traslato oltre lo strumento, cioe' un orizzonte
        // scomparso senza che nulla lo dichiari.
        horizon.annunciation = Annunciation::out_of_range;
        return horizon;
    }

    horizon.has_attitude = true;

    // Il quadro ruota in senso opposto al rollio: e' il mondo a inclinarsi.
    const double roll = normalise_roll(reading.roll.value);
    horizon.rotation_deg = -roll;
    horizon.bank_pointer_deg = roll;

    // A cabrare l'orizzonte scende, quindi il quadro trasla verso il basso.
    horizon.translation = pitch * radius_per_pitch_degree;

    // La scala si dirada con la dimensione, come le tacche dei quadranti: su un
    // display circolare piccolo i gradini ogni cinque gradi si toccherebbero, e
    // una scala illeggibile e peggio di una piu rada.
    const double half_diameter = sizing.diameter_px / 2.0;
    double step = rung_step_deg;
    while (step * radius_per_pitch_degree * half_diameter < sizing.minimum_tick_spacing_px &&
           step < ladder_limit_deg) {
        step *= 2.0;
    }
    const double labelled_step =
        10.0 * radius_per_pitch_degree * half_diameter < sizing.minimum_label_spacing_px
            ? 20.0
            : 10.0;

    for (double value = -ladder_limit_deg; value <= ladder_limit_deg + 1e-9; value += step) {
        if (std::abs(value) < 1e-9) continue;
        const bool major = std::fmod(std::abs(value), labelled_step) < 1e-9;
        Rung rung;
        rung.pitch_deg = value;
        rung.offset = value * radius_per_pitch_degree;
        rung.half_width = major ? major_rung_half_width : minor_rung_half_width;
        if (major) rung.text = std::to_string(static_cast<int>(std::abs(value)));
        horizon.ladder.push_back(rung);
    }

    return horizon;
}

TurnCoordinator describe_turn(const TurnReading& reading, const Sizing& sizing) {
    static_cast<void>(sizing);

    TurnCoordinator coordinator;
    coordinator.simulated = reading.turn_rate.simulated || reading.slip.simulated;

    // Le tacche della virata standard appartengono al quadrante: sono il
    // riferimento per cui esiste, e restano anche quando la sagoma non c'e'.
    for (const double side : {-1.0, 1.0}) {
        coordinator.standard_marks.push_back(
            Tick{side * aircraft_bank_at_limit_deg *
                     (standard_rate_deg_s / turn_display_limit_deg_s),
                 0.72, 0.95, TickKind::major});
    }

    if (is_usable(reading.turn_rate.validity)) {
        coordinator.has_turn = true;
        // La sagoma si inclina in proporzione al rateo, fino al limite del
        // quadrante. Oltre, resta al limite: farla ruotare oltre la verticale
        // renderebbe illeggibile proprio la virata piu' violenta.
        const double clamped = std::clamp(reading.turn_rate.value,
                                          -turn_display_limit_deg_s,
                                          turn_display_limit_deg_s);
        coordinator.aircraft_bank_deg =
            clamped / turn_display_limit_deg_s * aircraft_bank_at_limit_deg;
    }

    if (is_usable(reading.slip.validity)) {
        coordinator.has_ball = true;
        coordinator.ball_offset =
            std::clamp(reading.slip.value / slip_full_scale_g, -1.0, 1.0);
    }

    // Le due grandezze sono indipendenti e possono guastarsi separatamente. La
    // dicitura compare solo se manca tutto: con una sola delle due lo strumento
    // conserva meta' della propria utilita', e dichiararlo guasto la butterebbe
    // via.
    if (!coordinator.has_turn && !coordinator.has_ball) {
        const bool stale = reading.turn_rate.validity == Validity::stale ||
                           reading.slip.validity == Validity::stale;
        coordinator.annunciation = stale ? Annunciation::stale : Annunciation::failed;
    }

    return coordinator;
}

}  // namespace efis::instruments
