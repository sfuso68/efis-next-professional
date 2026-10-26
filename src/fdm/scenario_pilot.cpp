#include "efis/fdm/scenario_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace efis::fdm {
namespace {

constexpr double radians_to_degrees = 180.0 / std::numbers::pi;

// Guadagni del pilota.
//
// Due criteri, e il secondo e' costato una riscrittura. Il primo e' che il
// compito sia tenere un profilo e non seguirlo al piede: un pilota aggressivo
// produce oscillazioni che finiscono nei dati come se fossero turbolenza, e chi
// legge una registrazione non ha modo di sapere che sono del controllo.
//
// Il secondo e' che il controllo deve restare stabile alla cadenza con cui il
// chiamante lo chiama, che non e' quella con cui il modello integra. Il banco
// pubblica a cinque, venti o cinquanta hertz e integra a cento: fra due
// chiamate del pilota il comando resta fermo per molti passi di integrazione.
// La prima versione aveva guadagni tarati come se il comando fosse continuo, e
// a cinque hertz oscillava da fondo corsa a fondo corsa a ogni campione,
// producendo una crociera che perdeva quattrocento piedi al minuto con il
// comando a sbattere. I guadagni qui sono quelli che restano stabili al piu'
// lento dei casi, e il limite di velocita' dei comandi sotto completa il
// lavoro.
constexpr double vertical_speed_gain_fpm_per_ft = 1.6;
constexpr double elevator_gain_per_deg = 0.022;
constexpr double elevator_rate_gain_per_deg_s = 0.020;

// Termine integrale sull'errore di assetto, per escursione al grado per
// secondo.
//
// Serve perche' un controllo proporzionale si ferma sempre un po' prima:
// l'errore residuo e' quello che produce il comando che tiene l'equilibrio, e
// per definizione non va a zero. In crociera questo costava quattordici piedi,
// che non importano a nessuno. Nella prova di stallo costava lo stallo: il
// pilota si fermava a nove gradi di incidenza con l'aeroplano in discesa
// stabile a cinquantasei nodi, perche' quella posizione di equilibratore era
// l'equilibrio del suo stesso anello. Un pilota vero continua a tirare finche'
// l'aeroplano non risponde, e tirare finche' non risponde e' un integrale.
constexpr double elevator_integral_gain_per_deg_s = 0.004;
constexpr double elevator_integral_limit = 0.55;
constexpr double bank_gain_deg_per_deg = 1.1;
constexpr double aileron_gain_per_deg = 0.025;
constexpr double aileron_rate_gain_per_deg_s = 0.016;
constexpr double rudder_gain_per_g = 1.2;
constexpr double runway_rudder_gain_per_deg = 0.08;
constexpr double throttle_gain_per_kt_s = 0.05;

// Velocita' massima dei comandi, in frazione di escursione al secondo.
//
// Non e' un artificio di stabilita': un comando vero ha una velocita' massima,
// data dalla mano del pilota o dalla servoassistenza, e due decimi di secondo
// per andare da un estremo all'altro sono gia' piu' di quanto una mano faccia.
// Che questo limite renda stabile anche il controllo e' una conseguenza, non la
// ragione.
constexpr double surface_rate_per_s = 2.5;
constexpr double throttle_rate_per_s = 0.5;

[[nodiscard]] double shortest_angle_deg(double target, double current) noexcept {
    double error = target - current;
    while (error > 180.0) {
        error -= 360.0;
    }
    while (error < -180.0) {
        error += 360.0;
    }
    return error;
}

// Avvicina il comando al valore richiesto rispettando la velocita' massima.
[[nodiscard]] double rate_limited(double current, double target, double rate_per_s,
                                  double dt_s) noexcept {
    const double limit = rate_per_s * dt_s;
    return current + std::clamp(target - current, -limit, limit);
}

}  // namespace

ScenarioPilot::ScenarioPilot(const Airframe& airframe) noexcept
    : airframe_(&airframe) {}

void ScenarioPilot::fly(Vehicle& vehicle, double dt_s) noexcept {
    const VehicleState& truth = vehicle.state();
    Controls controls = vehicle.controls();

    if (!initialised_) {
        // Si parte da dove l'aeroplano e' gia': manetta e trim arrivano dal
        // posizionamento, che li ha stimati per l'equilibrio. Partire da zero
        // significherebbe togliere la potenza nell'istante in cui il pilota
        // prende i comandi, e il primo minuto di ogni prova sarebbe la
        // ripresa da quella cantonata e non il profilo richiesto.
        throttle_ = controls.throttle;
        elevator_trim_ = controls.elevator_trim;
        initialised_ = true;
    }

    const double altitude_ft = truth.altitude_m * feet_per_metre;
    const double pitch_deg = truth.pitch_rad * radians_to_degrees;
    const double roll_deg = truth.roll_rad * radians_to_degrees;
    const double heading_deg = truth.heading_rad * radians_to_degrees;
    const double alpha_deg = truth.alpha_rad * radians_to_degrees;
    const double calibrated_kt = truth.calibrated_airspeed_m_s *
                                 knots_per_metre_per_second;
    const double true_airspeed_fpm = truth.true_airspeed_m_s *
                                     feet_per_minute_per_metre_per_second;

    // --- Beccheggio --------------------------------------------------------
    //
    // Un solo anello, anche per lo stallo. La prova di stallo si conduce
    // tenendo la quota a motore ridotto e lasciando decadere la velocita' di
    // circa un nodo al secondo: l'incidenza sale da sola, perche' e' l'unica
    // via che resta per sostenere il peso, e l'ala cede quando raggiunge il
    // proprio massimo. Due versioni precedenti hanno provato a tirare
    // l'equilibratore con una rampa, ed e' sbagliato in modo istruttivo: a
    // centoventi nodi la stessa posizione che a cinquanta porta allo stallo
    // produce una cabrata a cinquantasette gradi e una salita a zoom. Il
    // comando non e' il fenomeno; la condizione lo e'.
    const double altitude_error_ft = targets_.altitude_ft - altitude_ft;
    const double commanded_vertical_speed =
        std::clamp(altitude_error_ft * vertical_speed_gain_fpm_per_ft,
                   -targets_.max_vertical_speed_fpm, targets_.max_vertical_speed_fpm);

    // Dall'obiettivo di rateo all'assetto, per via geometrica e non per via di
    // un altro anello: il rateo di salita e' la velocita' vera per il seno
    // della traiettoria, e la traiettoria e' l'assetto meno l'incidenza.
    // Chiedere direttamente l'assetto che produce quel rateo evita un
    // integratore, e un integratore in meno e' un modo in meno di oscillare.
    const double path_ratio = true_airspeed_fpm > 1.0
                                  ? std::clamp(commanded_vertical_speed /
                                                   true_airspeed_fpm,
                                               -0.35, 0.35)
                                  : 0.0;
    const double path_command_deg = std::asin(path_ratio) * radians_to_degrees;

    // L'incidenza entra nel precomando limitata a cio' che l'ala puo'
    // sostenere, piu' due gradi. Il limite superiore non e' prudenza: senza di
    // esso un'incidenza da stallo profondo -- trenta o quaranta gradi, con
    // l'aeroplano che scende quasi a piombo -- chiederebbe un assetto
    // altrettanto cabrato, cioe' esattamente il comando che tiene l'aeroplano
    // nello stallo invece di tirarlo fuori. Con il limite, il pilota continua
    // a tenere la quota fino al cedimento e non oltre.
    const double feed_forward_alpha_deg =
        std::clamp(alpha_deg, -4.0, airframe_->aero.stall_alpha_deg + 2.0);
    double pitch_command_deg = std::clamp(path_command_deg + feed_forward_alpha_deg,
                                          -12.0, 15.0);

    // Stallo in corso: si riduce l'incidenza, che e' la sola manovra che lo
    // interrompe. Picchiare resta la risposta anche se la quota obiettivo e'
    // sopra: con l'ala stallata la quota non si tiene comunque, e insistere a
    // cabrare la fa perdere piu' in fretta.
    if (truth.stalled) {
        pitch_command_deg = -6.0;
    }

    // A terra, prima della rotazione, l'equilibratore sta neutro: una cabrata
    // con il ruotino a terra non fa alzare il muso, lo carica.
    if (truth.on_ground && calibrated_kt < airframe_->limits.stall_clean_kcas * 1.05) {
        pitch_command_deg = 0.0;
    }

    const double pitch_error_deg = pitch_command_deg - pitch_deg;
    if (truth.stalled) {
        // Con l'ala stallata l'integrale accumulato e' il comando che ha
        // portato allo stallo: tenerlo significherebbe insistere. Si azzera,
        // ed e' il gesto giusto anche per un pilota vero: si rilascia.
        pitch_integral_ = 0.0;
    } else {
        pitch_integral_ = std::clamp(pitch_integral_ + pitch_error_deg * dt_s *
                                                           elevator_integral_gain_per_deg_s,
                                     -elevator_integral_limit,
                                     elevator_integral_limit);
    }

    const double elevator_command = std::clamp(
        pitch_error_deg * elevator_gain_per_deg + pitch_integral_ -
            truth.pitch_rate_rad_s * radians_to_degrees * elevator_rate_gain_per_deg_s,
        -1.0, 1.0);

    // Il trim segue lentamente il comando, come una mano che si scarica. Serve
    // a tenere l'equilibratore vicino al centro: una superficie costantemente a
    // fondo corsa saturerebbe, e il controllo perderebbe autorita' proprio
    // quando arriva una raffica.
    elevator_trim_ += (elevator_command - elevator_trim_) * std::min(dt_s / 10.0, 1.0);
    controls.elevator_trim = std::clamp(elevator_trim_, -0.7, 0.7);
    controls.elevator = rate_limited(controls.elevator,
                                     std::clamp(elevator_command -
                                                    controls.elevator_trim,
                                                -1.0, 1.0),
                                     surface_rate_per_s, dt_s);

    // --- Rollio e direzione ------------------------------------------------
    const double heading_error_deg = shortest_angle_deg(targets_.heading_deg,
                                                        heading_deg);
    if (targets_.on_runway) {
        // In pista si tiene l'asse con il piede e le ali a livello. Il limite
        // di dieci gradi sull'errore evita che una partenza con prua molto
        // diversa dia un comando di piede a fondo corsa.
        controls.aileron = rate_limited(controls.aileron,
                                        std::clamp(-roll_deg * aileron_gain_per_deg,
                                                   -0.3, 0.3),
                                        surface_rate_per_s, dt_s);
        controls.rudder = rate_limited(controls.rudder,
                                       std::clamp(std::clamp(heading_error_deg, -10.0,
                                                             10.0) *
                                                      runway_rudder_gain_per_deg,
                                                  -1.0, 1.0),
                                       surface_rate_per_s, dt_s);
    } else {
        const double bank_command_deg = std::clamp(heading_error_deg *
                                                       bank_gain_deg_per_deg,
                                                   -25.0, 25.0);
        const double aileron_command = std::clamp(
            (bank_command_deg - roll_deg) * aileron_gain_per_deg -
                truth.roll_rate_rad_s * radians_to_degrees *
                    aileron_rate_gain_per_deg_s,
            -1.0, 1.0);
        controls.aileron = rate_limited(controls.aileron, aileron_command,
                                        surface_rate_per_s, dt_s);

        // Pallina al centro: il piede annulla l'accelerazione laterale. E' il
        // modo in cui si coordina una virata davvero, e produce
        // un'accelerazione laterale quasi nulla sul display, che e' cio' che un
        // EFIS deve mostrare in virata corretta.
        controls.rudder = rate_limited(controls.rudder,
                                       std::clamp(-truth.accel_y_g * rudder_gain_per_g,
                                                  -1.0, 1.0),
                                       surface_rate_per_s, dt_s);
    }

    // --- Potenza -----------------------------------------------------------
    double throttle_command = throttle_ + (targets_.airspeed_kcas - calibrated_kt) *
                                              throttle_gain_per_kt_s * dt_s;

    // In salita la manetta sta tutta avanti: un aeroplano leggero non ha
    // potenza da sprecare, e un controllo di velocita' che la riducesse in
    // salita produrrebbe una salita che non assomiglia a nessuna salita vera.
    if (!targets_.stall_entry && targets_.altitude_ft - altitude_ft > 200.0) {
        throttle_command = 1.0;
    }
    if (targets_.stall_entry) {
        throttle_command = 0.0;
    }
    throttle_ = rate_limited(throttle_, std::clamp(throttle_command, 0.0, 1.0),
                             throttle_rate_per_s, dt_s);
    controls.throttle = throttle_;

    controls.flap_deg = targets_.flap_deg;
    controls.gear_down = airframe_->gear.retractable ? targets_.gear_down : true;
    controls.propeller = 1.0;
    controls.mixture = 1.0;

    vehicle.set_controls(controls);
}

}  // namespace efis::fdm
