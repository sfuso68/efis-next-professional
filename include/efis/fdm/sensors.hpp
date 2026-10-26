#pragma once

#include "efis/fdm/atmosphere.hpp"
#include "efis/fdm/dynamics.hpp"

#include <cstdint>

namespace efis::fdm {

// Strato dei sensori: dalla verita' del modello a cio' che l'EFIS riceve.
//
// E' la parte del banco che conta di piu', e la ragione e' in ADR-004. Un
// simulatore che pubblicasse direttamente la verita' del modello non potrebbe
// provare nulla di cio' che quell'ADR ha introdotto: la stasi, il confronto
// incrociato, il dato plausibile ma falso. Quei meccanismi esistono per
// accorgersi di un sensore che mente, e per provarli serve un sensore che
// mente.
//
// Quindi qui, e solo qui, ci sono errore, ritardo, risoluzione, rumore e
// guasto. Separare questo strato dalla dinamica ha una conseguenza che vale da
// sola il confine: in ogni istante il banco conosce entrambe le versioni della
// stessa grandezza, quella vera e quella misurata, e la differenza fra le due
// e' l'errore dello strumento, misurato e non supposto.
//
// Il rumore riproduce quello misurato sul banco reale il 21 agosto 2026 con
// tools/tuning/measure_signal_noise.py: 0,44 nodi e 2,00 piedi al 99esimo
// percentile fra campioni consecutivi. Non e' un dettaglio di realismo: le
// risoluzioni dello StasisMonitor sono tarate su quei numeri, e un banco piu'
// silenzioso del vero farebbe passare per buona una taratura che in volo non
// scatterebbe.

enum class SensorFault : std::uint8_t {
    none,

    // Pitot ostruito: la pressione totale resta quella intrappolata nel
    // momento dell'ostruzione. In salita la velocita' indicata cresce da sola,
    // perche' la statica cala mentre la totale non cambia. E' il guasto che si
    // presenta come un anemometro perfettamente funzionante.
    pitot_blocked,

    // Perdita sulla linea del Pitot: la pressione d'impatto arriva ridotta e
    // la velocita' legge meno del vero. Non si ferma, non si azzera: sbaglia.
    pitot_leak,

    // Presa statica ostruita: la quota si ferma al valore del momento, e la
    // velocita' indicata sbaglia nel verso opposto in salita e in discesa.
    static_blocked,

    // Inerziale latchato: assetto e prua restano all'ultimo valore prodotto,
    // con il proprio rumore. E' il caso principale di ADR-004.
    ahrs_latched,

    // Inerziale che deriva: la prua si allontana lentamente dal vero. Nessun
    // singolo campione e' sbagliato in modo evidente.
    ahrs_drift,

    // Satellitare assente: la posizione non c'e'. Dichiararla assente e' il
    // comportamento corretto, ed e' il caso in cui l'EFIS deve togliere il
    // dato invece di estrapolarlo.
    gnss_outage,

    // Satellitare degradato: posizione presente ma con dispersione molto
    // maggiore e pochi satelliti. Il dato c'e' e vale meno.
    gnss_degraded,

    // Magnetometro con offset: la prua magnetica sbaglia di un valore fisso.
    // Il confronto con la rotta satellitare e' il solo modo di vederlo.
    magnetometer_bias,

    // Sonda di temperatura guasta: la temperatura esterna resta a un valore
    // plausibile e sbagliato, e con essa sbaglia la velocita' vera che se ne
    // ricava.
    oat_stuck
};

[[nodiscard]] const char* sensor_fault_name(SensorFault fault) noexcept;

// Taratura dello strato sensori. Valori separati per grandezza, perche' i
// sensori sono fisicamente diversi e nessuna media li descrive.
struct SensorPolicy final {
    // Costanti di tempo delle linee pneumatiche. Il ritardo esiste: il tubo ha
    // volume e la presa ha diametro, e in salita rapida la quota indicata
    // arriva dopo. Un EFIS tarato senza ritardo dichiara un disaccordo fra
    // quota barometrica e satellitare che e' solo il proprio ritardo.
    double pitot_lag_s{0.30};
    double static_lag_s{0.50};

    // Errore di posizione della presa statica, in frazione della pressione
    // dinamica: un termine costante piu' uno proporzionale all'incidenza. E' il
    // motivo per cui la velocita' indicata non coincide con la calibrata, e il
    // motivo per cui la differenza cresce avvicinandosi allo stallo, dove conta
    // di piu'.
    double static_position_error{-0.012};
    double static_position_error_alpha{0.090};

    // Deviazione standard del rumore. Ricavata dal 99esimo percentile misurato
    // sul banco reale: per una differenza fra campioni indipendenti quel
    // percentile vale circa 3,3 sigma, quindi 0,44 nodi danno 0,13 e 2,00
    // piedi danno 0,60.
    double airspeed_noise_kt{0.13};
    double altitude_noise_ft{0.60};
    double attitude_noise_deg{0.04};
    double rate_noise_deg_s{0.05};
    double acceleration_noise_g{0.004};

    // Risoluzione di pubblicazione dei sensori, non del display: cio' che il
    // convertitore quantizza. Resta sotto la risoluzione dichiarata nello
    // StasisMonitor, che e' una soglia di decisione e non una quantizzazione.
    double airspeed_resolution_kt{0.1};
    double altitude_resolution_ft{1.0};

    // Deriva dell'inerziale sulla prua, in gradi al minuto, quando il guasto di
    // deriva e' attivo.
    double heading_drift_deg_min{3.0};

    // Cadenza e ritardo del satellitare. Cinque hertz e due decimi di secondo
    // sono valori da ricevitore di classe avionica leggera. Il ritardo e'
    // l'ingrediente che rende non banale il confronto fra velocita' al suolo e
    // velocita' indicata in accelerazione.
    double gnss_rate_hz{5.0};
    double gnss_latency_s{0.20};
    double gnss_position_sigma_m{1.5};
    double gnss_vertical_speed_sigma_fpm{30.0};

    // Fattore di recupero della sonda di temperatura totale. Una sonda
    // immersa nel flusso misura la temperatura totale, non la statica, e la
    // differenza a 250 nodi vale quasi dieci gradi: ignorarla sbaglia la
    // velocita' vera e la quota di densita'.
    double temperature_recovery_factor{0.85};
};

// Cio' che l'EFIS riceve. Le unita' sono quelle dello strumento, non quelle del
// modello: e' il confine in cui si fa la conversione, una volta sola.
struct IndicatedState final {
    double indicated_airspeed_kt{};
    double pressure_altitude_ft{};
    double indicated_altitude_ft{};
    double baro_setting_hpa{1013.25};
    double vertical_speed_fpm{};
    double differential_pressure_pa{};
    double static_pressure_pa{};
    double total_air_temperature_c{};
    double mach{};
    bool air_data_valid{true};

    double roll_deg{};
    double pitch_deg{};
    double magnetic_heading_deg{};
    double true_heading_deg{};
    double turn_rate_deg_s{};
    double lateral_acceleration_g{};
    double normal_acceleration_g{1.0};
    double angle_of_attack_deg{};
    bool attitude_valid{true};

    double latitude_deg{};
    double longitude_deg{};
    double gnss_altitude_ft{};
    double ground_speed_kt{};
    double track_deg{};
    double gnss_vertical_speed_fpm{};
    bool gnss_valid{true};
    int satellites{12};
    double horizontal_dilution{0.9};
};

class SensorSuite final {
public:
    explicit SensorSuite(SensorPolicy policy = {}, std::uint64_t seed = 20261026) noexcept;

    void set_fault(SensorFault fault) noexcept;
    [[nodiscard]] SensorFault fault() const noexcept { return fault_; }

    // Regolazione dell'altimetro impostata dal pilota. Non e' una misura, e il
    // modello dati la tiene distinta per questo: una regolazione sbagliata
    // produce una quota indicata sbagliata senza che nessun sensore sia guasto.
    void set_baro_setting_hpa(double value) noexcept { baro_setting_hpa_ = value; }
    [[nodiscard]] double baro_setting_hpa() const noexcept { return baro_setting_hpa_; }

    // Declinazione del luogo. Arriva dall'ambiente e non dalla taratura dei
    // sensori perche' non e' una proprieta' del magnetometro: lo stesso
    // strumento legge prue magnetiche diverse in posti diversi. Senza di essa
    // la prua magnetica coinciderebbe con quella vera e il confronto fra prua e
    // rotta satellitare non proverebbe nulla.
    void set_magnetic_variation_deg(double value) noexcept {
        magnetic_variation_deg_ = value;
    }

    // Riallinea i sensori allo stato vero, azzerando ritardi e memorie. Serve
    // quando il banco riposiziona l'aeroplano: senza questo, dopo un salto di
    // quota le linee pneumatiche inseguirebbero per secondi un valore che
    // nessun volo vero ha prodotto.
    void align(const VehicleState& truth, const AirState& air) noexcept;

    const IndicatedState& update(const VehicleState& truth, const AirState& air,
                                 double dt_s) noexcept;

    [[nodiscard]] const IndicatedState& indicated() const noexcept { return indicated_; }

private:
    [[nodiscard]] double gaussian() noexcept;
    [[nodiscard]] double uniform() noexcept;

    SensorPolicy policy_;
    SensorFault fault_{SensorFault::none};
    IndicatedState indicated_{};

    std::uint64_t random_state_{};
    double baro_setting_hpa_{1013.25};
    double magnetic_variation_deg_{3.0};
    double stuck_temperature_c_{15.0};

    // Stato pneumatico filtrato, in pascal. Il ritardo si modella sulle
    // pressioni e non sulle grandezze indicate, perche' e' li' che avviene: a
    // ritardare e' il tubo, non il calcolo.
    double filtered_total_pa_{};
    double filtered_static_pa_{};
    double trapped_total_pa_{};
    double trapped_static_pa_{};
    bool initialised_{};

    // Memoria del variometro: la velocita' verticale barometrica si ottiene
    // derivando la quota, e la derivata di un valore quantizzato ha bisogno di
    // un filtro. Un variometro vero e' una capacita' con un capillare, cioe'
    // esattamente questo.
    double previous_pressure_altitude_ft_{};
    double filtered_vertical_speed_fpm_{};

    double heading_drift_deg_{};
    double latched_roll_deg_{};
    double latched_pitch_deg_{};
    double latched_heading_deg_{};
    bool latched_{};

    // Verita' ritardata che il ricevitore sta elaborando. Il ritardo e'
    // modellato come filtro del primo ordine e non come coda di campioni:
    // sposta il valore nel tempo della quantita' giusta, ma lo addolcisce,
    // cosa che un ritardo puro non fa. Per una prova di confronto fra catene
    // e' la parte che conta; per misurare la risposta a un gradino non lo e',
    // e va saputo.
    double delayed_latitude_deg_{};
    double delayed_longitude_deg_{};
    double delayed_ground_speed_kt_{};
    double delayed_track_deg_{};
    double delayed_vertical_speed_fpm_{};
    double delayed_altitude_ft_{};

    double gnss_timer_s_{};
    double gnss_hold_latitude_deg_{};
    double gnss_hold_longitude_deg_{};
    double gnss_hold_ground_speed_kt_{};
    double gnss_hold_track_deg_{};
    double gnss_hold_vertical_speed_fpm_{};
    double gnss_hold_altitude_ft_{};
};

}  // namespace efis::fdm
