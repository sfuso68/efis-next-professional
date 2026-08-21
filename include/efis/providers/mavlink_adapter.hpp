#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/node_health.hpp"
#include "efis/providers/mavlink_parser.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace efis {

// Stato del sensore aria, oltre la sola velocita' indicata.
//
// Il Matek ASPD-DLVR misura una pressione differenziale e la temperatura
// dell'elemento; la velocita' e' cio' che il controllo di volo ne ricava. Tenere
// separate le due cose non e' pedanteria: al banco serve la grandezza misurata,
// perche' e' quella che si confronta con un riferimento e su cui si fa la
// taratura. Vedi paragrafo 14 dell'handoff.
struct AirDataSensorState final {
    Sample<double> differential_pressure_hpa;
    Sample<double> element_temperature_c;

    // Velocita' calcolata dal controllo di volo a partire dalla pressione.
    Sample<double> computed_airspeed_kt;

    // Il controllo di volo dichiara presente il sensore di pressione
    // differenziale.
    bool declared_present{};

    // Il controllo di volo lo dichiara sano. Con ARSPD_USE a zero puo' essere
    // falso pur essendo il sensore perfettamente funzionante: dal solo bit non
    // si distingue "non usato" da "guasto", e per questo la velocita' che ne
    // deriva non viene presentata come valida.
    bool declared_healthy{};
};

// Cio' che l'adapter ha incontrato, oltre ai valori.
// Autodiagnosi del filtro di stato dell'autopilota.
//
// ArduPilot confronta gia' fra loro le proprie sorgenti e pubblica il risultato
// di quel confronto. Ignorarlo significherebbe rifare peggio, con meno dati, un
// lavoro gia' fatto meglio: e' il confronto incrociato piu' economico che
// abbiamo, e finora veniva decodificato e buttato.
//
// Sui bit di stato si decide, sulle varianze no. I bit sono un giudizio che
// l'autopilota esprime esplicitamente; le varianze sono numeri di cui il
// protocollo non dichiara la soglia oltre la quale sono cattive. Inventarne una
// qui produrrebbe un allarme il cui significato nessuno saprebbe interpretare,
// quindi vengono registrate per la diagnosi e la black box, e non decidono
// nulla.
struct EstimatorState final {
    // L'autopilota ha riportato almeno una volta. Falso non significa sano:
    // significa che non si sa, ed e' una condizione diversa.
    bool reported{};

    std::uint32_t flags{};

    // Un bit acceso significa che quella stima e' sana e disponibile.
    bool attitude_healthy{};
    bool horizontal_velocity_healthy{};
    bool vertical_velocity_healthy{};
    bool horizontal_position_healthy{};

    // Il filtro non e' inizializzato: non esiste alcuna stima, il che e'
    // diverso da una stima cattiva.
    bool uninitialised{};

    // Navigazione a stima: l'autopilota tiene la posizione ferma perche' non ha
    // di meglio. La posizione continua a essere pubblicata e non descrive piu'
    // dove si trova l'aeromobile.
    bool dead_reckoning{};

    // Il satellitare sta saltando. Il fix resta dichiarato valido, ed e' per
    // questo che il bit serve: e' l'unico modo di sapere che non lo e'.
    bool gnss_glitching{};

    // Registrate, non usate per decidere. La varianza sulla velocita' vera
    // dell'aria e' il giudizio che l'autopilota da' proprio sul sensore Matek.
    double velocity_variance{};
    double horizontal_position_variance{};
    double vertical_position_variance{};
    double compass_variance{};
    double airspeed_variance{};
};

// Vibrazioni misurate dagli accelerometri.
//
// Contano perche' una vibrazione forte degrada l'assetto prima ancora che il
// filtro se ne accorga, e i contatori di saturazione dicono che qualche
// campione e' stato scartato del tutto. Non tornano mai indietro: conta la loro
// crescita, non il valore assoluto.
struct VibrationState final {
    bool reported{};
    double x_axis{};
    double y_axis{};
    double z_axis{};
    std::uint32_t clipping_0{};
    std::uint32_t clipping_1{};
    std::uint32_t clipping_2{};

    // Vero se un contatore e' cresciuto dall'ultimo messaggio: e' il momento in
    // cui l'accelerometro ha saturato, non il fatto che sia saturato in passato.
    bool clipping_increasing{};
};

struct MavlinkAdapterStatistics final {
    std::uint64_t frames_applied{};

    // Frame integri di un messaggio che l'adapter non traduce in campioni.
    // Non e' un errore: la tabella copre piu' messaggi di quelli usati.
    std::uint64_t frames_ignored{};

    // Numero di volte in cui il contatore di sequenza a otto bit ha girato.
    // MAVLink lo azzera ogni 256 messaggi: interpretarlo alla lettera farebbe
    // apparire una regressione della sequenza a ogni giro.
    std::uint64_t sequence_wraps{};
};

// Traduce i messaggi del Pixhawk in campioni con provenienza completa.
//
// Adapter di sola lettura: consuma byte e non ne produce. Non scrive parametri,
// non richiede stream, non comanda nulla. Corrisponde al primo livello della
// scala del paragrafo 25 dell'handoff.
//
// Le regole di validita' non sono un dettaglio implementativo ma il motivo per
// cui questa classe esiste: senza di esse un valore ricevuto verrebbe mostrato
// per il solo fatto di essere arrivato.
class MavlinkAdapter final {
public:
    explicit MavlinkAdapter(NodeHealthPolicy policy = {});

    // Consuma byte grezzi dalla seriale o dalla rete. `now` e' il tempo monotono
    // locale di ricezione.
    void consume(std::span<const std::uint8_t> bytes, MonoTime now);

    [[nodiscard]] const FlightSnapshot& snapshot() const noexcept { return snapshot_; }
    [[nodiscard]] const EstimatorState& estimator() const noexcept {
        return estimator_;
    }

    [[nodiscard]] const VibrationState& vibration() const noexcept {
        return vibration_;
    }

    [[nodiscard]] const AirDataSensorState& air_data_sensor() const noexcept {
        return air_data_;
    }
    [[nodiscard]] const NodeHealthMonitor& health() const noexcept { return health_; }
    [[nodiscard]] const mavlink::ParserStatistics& parser_statistics() const noexcept {
        return parser_.statistics();
    }
    [[nodiscard]] const MavlinkAdapterStatistics& statistics() const noexcept {
        return statistics_;
    }

    // Transizioni di stato dei nodi accumulate, destinate alla black box.
    [[nodiscard]] const std::vector<NodeStateChange>& node_events() const noexcept {
        return node_events_;
    }
    void clear_node_events() noexcept { node_events_.clear(); }

    // Ultimo avviso ricevuto dal controllo di volo, per esempio un motivo di
    // mancato pre-arm.
    // Stato del satellitare. Non e' il criterio di validita', che resta nella
    // validita' dei campioni: e' cio' che permette di dire perche' una posizione
    // non e' utilizzabile, invece di mostrarla soltanto spenta.
    [[nodiscard]] std::int64_t gnss_fix_type() const noexcept { return gnss_fix_type_; }
    [[nodiscard]] std::int64_t gnss_satellites() const noexcept {
        return gnss_satellites_;
    }

    [[nodiscard]] const std::string& last_status_text() const noexcept {
        return last_status_text_;
    }

    // Applica i timeout ai nodi. Va chiamata anche quando non arriva nulla:
    // e' il silenzio a dover produrre un evento.
    void update(MonoTime now);

    // Identificativo di nodo derivato dall'indirizzo MAVLink. Sistema e
    // componente insieme identificano il trasmettitore: due componenti dello
    // stesso sistema sono due sorgenti distinte e vanno sorvegliate separatamente.
    [[nodiscard]] static NodeId node_of(std::uint8_t system, std::uint8_t component) noexcept {
        return static_cast<NodeId>((static_cast<NodeId>(system) << 8) | component);
    }

private:
    void apply(const mavlink::Frame& frame, MonoTime now);
    void track(const mavlink::Frame& frame, MonoTime now);
    void revalidate();

    mavlink::Parser parser_;
    NodeHealthMonitor health_;
    FlightSnapshot snapshot_{};
    AirDataSensorState air_data_{};
    MavlinkAdapterStatistics statistics_{};
    std::vector<NodeStateChange> node_events_;
    std::string last_status_text_;

    // Estensione a 64 bit del contatore di sequenza a otto bit, per nodo.
    struct SequenceTracker final {
        NodeId node{};
        std::uint8_t last{};
        std::uint64_t extended{};
        bool started{};
    };
    std::vector<SequenceTracker> sequences_;

    // Ultimo stato noto del satellitare e del sensore differenziale. Servono a
    // decidere la validita' di campioni che arrivano in messaggi diversi da
    // quelli che portano lo stato.
    std::int64_t gnss_fix_type_{};
    std::int64_t gnss_satellites_{};
    bool differential_pressure_healthy_{};
    EstimatorState estimator_{};
    VibrationState vibration_{};
    UtcMicros utc_{utc_unavailable};
};

// Fix tridimensionale: soglia sotto la quale una posizione ricevuta non e' una
// posizione. Vedi paragrafo 13 dell'handoff.
inline constexpr std::int64_t gnss_fix_3d = 3;

// MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE
inline constexpr std::uint64_t sensor_bit_differential_pressure = 0x20;

}  // namespace efis
