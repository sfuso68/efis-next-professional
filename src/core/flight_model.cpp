#include "efis/core/flight_model.hpp"
#include "efis/core/alert_engine.hpp"

#include <array>

namespace efis {
namespace {

template <class T>
void age(Sample<T>& sample, MonoTime now, std::chrono::milliseconds limit,
         SystemHealth& health) {
    // Un campo che nessuna sorgente ha mai pubblicato non e' un campo guasto:
    // e' un campo che nessuno fornisce. La distinzione e' la stessa che vale per
    // i nodi fra "mai sentito" e "perduto", e conta per la stessa ragione. Senza
    // di essa basterebbe aggiungere una grandezza al modello, o collegare un
    // autopilota che non trasmette un certo messaggio, per tenere la salute in
    // allarme per sempre a causa di un dato che nessuno aveva promesso. Un
    // allarme permanente e' un allarme che nessuno guarda piu'.
    //
    // Una perdita vera resta contata: un campione pubblicato conserva il proprio
    // segnale d'origine anche quando la sorgente tace.
    if (sample.origin.signal == SignalId::unknown) return;

    if (sample.validity == Validity::unavailable) {
        ++health.unavailable_fields;
        return;
    }
    if (now - sample.received_at > limit) {
        sample.validity = Validity::stale;
    }
    if (sample.validity == Validity::stale) {
        ++health.stale_fields;
    }
}

}  // namespace

FlightModel::FlightModel(FlightModelPolicy policy)
    : policy_(policy),
      speed_check_(policy.speed_agreement),
      vertical_check_(policy.vertical_agreement) {
    configure_stasis();
}

FlightModel::FlightModel(std::chrono::milliseconds stale_after)
    : FlightModel(FlightModelPolicy{.stale_after = stale_after}) {}

void FlightModel::configure_stasis() {
    stasis_.configure(SignalId::indicated_airspeed,
                      {.suspect_after = policy_.air_data_suspect_after,
                       .resolution = policy_.airspeed_resolution_kt,
                       .applicable = true});
    stasis_.configure(SignalId::pressure_altitude,
                      {.suspect_after = policy_.air_data_suspect_after,
                       .resolution = policy_.altitude_resolution_ft,
                       .applicable = true});
    stasis_.configure(SignalId::roll,
                      {.suspect_after = policy_.attitude_suspect_after,
                       .resolution = policy_.attitude_resolution_deg,
                       .applicable = true});
    stasis_.configure(SignalId::pitch,
                      {.suspect_after = policy_.attitude_suspect_after,
                       .resolution = policy_.attitude_resolution_deg,
                       .applicable = true});
    stasis_.configure(SignalId::ground_speed,
                      {.suspect_after = policy_.ground_speed_suspect_after,
                       .resolution = policy_.ground_speed_resolution_kt,
                       .applicable = true});

    // Prua e velocita' verticale sono deliberatamente escluse. Con un pilota
    // automatico che tiene bene, entrambe possono restare dentro la risoluzione
    // dello strumento per parecchi secondi senza che nulla sia guasto: una
    // soglia le farebbe gridare durante un volo perfettamente sano. Andranno
    // sorvegliate con un criterio diverso, non con questo.
}

FlightSnapshot FlightModel::normalize(FlightSnapshot input, MonoTime now) {
    age(input.attitude.roll_deg, now, policy_.stale_after, input.health);
    age(input.attitude.pitch_deg, now, policy_.stale_after, input.health);
    age(input.attitude.heading_deg, now, policy_.stale_after, input.health);
    age(input.air_data.indicated_airspeed_kt, now, policy_.stale_after, input.health);
    age(input.air_data.pressure_altitude_ft, now, policy_.stale_after, input.health);
    age(input.air_data.vertical_speed_fpm, now, policy_.stale_after, input.health);
    age(input.position.latitude_deg, now, policy_.stale_after, input.health);
    age(input.position.longitude_deg, now, policy_.stale_after, input.health);
    age(input.position.ground_speed_kt, now, policy_.stale_after, input.health);
    age(input.position.gnss_vertical_speed_fpm, now, policy_.stale_after, input.health);
    age(input.guidance.flight_director_active, now, policy_.stale_after, input.health);
    age(input.guidance.commanded_roll_deg, now, policy_.stale_after, input.health);
    age(input.guidance.commanded_pitch_deg, now, policy_.stale_after, input.health);
    age(input.guidance.selected_altitude_ft, now, policy_.stale_after, input.health);
    age(input.guidance.selected_course_deg, now, policy_.stale_after, input.health);
    age(input.guidance.selected_airspeed_kt, now, policy_.stale_after, input.health);
    age(input.navigation.cdi_deviation_dots, now, policy_.stale_after, input.health);
    age(input.navigation.glide_slope_dots, now, policy_.stale_after, input.health);
    age(input.navigation.to_flag, now, policy_.stale_after, input.health);

    // Se l'aeromobile sia in moto si stabilisce adesso, prima di ogni altra
    // valutazione. Il confronto qui sotto puo' marcare la velocita' al suolo
    // come inutilizzabile, e leggerla dopo farebbe chiudere il cancello del
    // moto: una protezione ne disattiverebbe un'altra in silenzio, che e'
    // esattamente il genere di interazione che non deve esistere.
    const auto& ground = input.position.ground_speed_kt;
    const bool in_motion = ground.usable() && ground.value >= policy_.motion_threshold_kt;

    // Confronto fra catene indipendenti. Va eseguito dopo l'invecchiamento, cosi'
    // un campione gia' scaduto non viene usato come termine di paragone: il
    // disaccordo che ne risulterebbe dipenderebbe dal guasto gia' rilevato.
    const auto agreement = speed_check_.compare(input.air_data.indicated_airspeed_kt,
                                                input.position.ground_speed_kt, now);
    if (agreement.agreement == Agreement::disagree) {
        apply_disagreement(agreement, input.air_data.indicated_airspeed_kt,
                           input.position.ground_speed_kt);
        input.health.disagreeing_fields += 2;
    }

    // Velocita' verticale barometrica contro satellitare. E' il confronto che
    // ADR-004 indicava come necessario e che finora mancava, e intercetta la
    // presa statica ostruita: il barometro smette di seguire la quota mentre il
    // satellitare continua a vederla cambiare.
    //
    // Si confrontano le velocita' e non le quote. La quota barometrica e quella
    // satellitare differiscono sempre, per regolazione barometrica e separazione
    // del geoide, e uno scarto costante di centinaia di piedi e' normale: un
    // confronto sui valori assoluti direbbe "disaccordo" a ogni volo. Le loro
    // variazioni invece devono coincidere, perche' descrivono lo stesso moto.
    const auto vertical = vertical_check_.compare(input.air_data.vertical_speed_fpm,
                                                  input.position.gnss_vertical_speed_fpm,
                                                  now);
    if (vertical.agreement == Agreement::disagree) {
        apply_disagreement(vertical, input.air_data.vertical_speed_fpm,
                           input.position.gnss_vertical_speed_fpm);
        input.health.disagreeing_fields += 2;
    }

    // Sorveglianza del valore fermo, sospesa mentre l'aeromobile non e' in moto.
    //
    // A terra fermi, assetto quota e velocita' sono legittimamente costanti: ogni
    // soglia produrrebbe un allarme a ogni sosta, e un allarme che grida sempre
    // e' un allarme che nessuno guarda piu'. La sorveglianza si apre durante la
    // corsa di decollo.
    //
    // Se la velocita' al suolo fosse a sua volta ferma su zero mentre
    // l'aeromobile accelera davvero, questo cancello resterebbe chiuso. Non e'
    // una svista: quel caso lo intercetta il confronto qui sopra, perche' la
    // velocita' indicata crescerebbe mentre quella al suolo resta a zero. Le due
    // protezioni si coprono a vicenda proprio dove ciascuna e' cieca.
    if (in_motion) {
        const std::array<const Sample<double>*, 5> watched{
            &input.air_data.indicated_airspeed_kt,
            &input.air_data.pressure_altitude_ft,
            &input.attitude.roll_deg,
            &input.attitude.pitch_deg,
            &input.position.ground_speed_kt};

        for (const Sample<double>* sample : watched) {
            if (stasis_.observe(*sample, now) != Motion::suspect) continue;
            ++input.health.suspect_fields;
            // Si nomina la prima grandezza ferma nell'ordine in cui sono
            // elencate, che e' l'ordine di gravita': una velocita' ferma conta
            // piu' di una quota ferma, che conta piu' di un assetto fermo.
            if (input.health.frozen_signal == SignalId::unknown) {
                input.health.frozen_signal = sample->origin.signal;
            }
        }
    } else {
        // Fermi, la sorveglianza riparte da capo: il valore costante osservato a
        // terra non deve essere contato come immobilita' al decollo successivo.
        stasis_.reset();
    }

    if (input.health.unavailable_fields > 0 || input.health.disagreeing_fields > 0) {
        input.health.level = HealthLevel::warning;
    } else if (input.health.stale_fields > 0 || input.health.suspect_fields > 0) {
        input.health.level = HealthLevel::caution;
    } else {
        input.health.level = HealthLevel::normal;
    }
    input.timestamp = now;
    input.alerts = AlertEngine{}.evaluate(input);
    return input;
}

}  // namespace efis
