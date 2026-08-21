#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/node_health.hpp"
#include "efis/providers/can_frame.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace efis {

// Identificativi del protocollo, vedi docs/protocols/ESP32_CAN_V1.md.
namespace esp32_can {

inline constexpr std::uint32_t id_attitude = 0x100;
inline constexpr std::uint32_t id_air_data = 0x101;
inline constexpr std::uint32_t id_health = 0x200;
inline constexpr std::uint32_t id_diagnostics = 0x201;
inline constexpr std::uint32_t id_gps_position = 0x300;
inline constexpr std::uint32_t id_gps_velocity = 0x301;
inline constexpr std::uint32_t id_gps_status = 0x302;

// Comandi diretti al nodo. EFIS NEXT non li trasmette: l'identificativo esiste
// qui perche' vada riconosciuto e ignorato.
inline constexpr std::uint32_t id_command = 0x018;

}  // namespace esp32_can

// Stato dichiarato dal nodo sensori.
//
// Il nodo dice come sta, e va creduto: non oltre quello che dice, ma nemmeno
// meno. Ignorare queste dichiarazioni significherebbe presentare come misura
// l'uscita di una fusione non calibrata.
struct Esp32SensorState final {
    bool imu_ok{};
    bool barometer_ok{};
    bool pitot_ok{};

    // Calibrazione BNO055, da 0 a 3 per ciascun sottosistema.
    std::uint8_t calibration_system{};
    std::uint8_t calibration_magnetometer{};
    std::uint8_t calibration_gyroscope{};
    std::uint8_t calibration_accelerometer{};

    bool gnss_fix{};
    std::uint8_t gnss_satellites{};
    double gnss_hdop{};

    // Grandezze che il nodo misura e che il Pixhawk non fornisce nella stessa
    // forma. Il fattore di carico e' l'esempio: nessun messaggio MAVLink
    // interpretato da EFIS NEXT lo porta.
    Sample<double> load_factor_g;
    Sample<double> temperature_c;
    Sample<double> differential_pressure_pa;
    Sample<double> static_pressure_hpa;

    // Quota satellitare, che il nodo trasmette in metri. Non ha un posto nello
    // snapshot di volo, dove la quota e' quella barometrica, e va tenuta
    // distinta: sono due grandezze diverse ottenute per vie diverse, ed e'
    // proprio la differenza a renderle confrontabili.
    Sample<double> gnss_altitude_ft;
    Sample<double> gnss_track_deg;

    // Diagnostica del nodo. Un modulo che si riavvia per watchdog in volo
    // trasmette valori corretti fra un riavvio e l'altro: senza questi conteggi
    // la cosa resterebbe invisibile.
    std::uint8_t firmware_major{};
    std::uint8_t firmware_minor{};
    std::uint8_t reset_reason{};
    std::uint8_t boot_count{};
    std::uint8_t crash_count{};
    std::uint16_t free_heap_kb{};
    bool diagnostics_received{};
};

struct Esp32AdapterStatistics final {
    std::uint64_t frames_applied{};

    // Frame di un identificativo che l'adapter non traduce, compresi i comandi
    // diretti al nodo. Non e' un errore.
    std::uint64_t frames_ignored{};

    // Frame piu' corti di quanto il messaggio richieda. Non vengono completati
    // con zeri: un payload troncato non e' una misura a zero.
    std::uint64_t frames_truncated{};

    // Avvolgimenti del contatore heartbeat, che e' a 16 bit. Interpretarli come
    // regressioni dichiarerebbe un guasto ogni 6553 secondi su un nodo sano.
    std::uint64_t heartbeat_wraps{};
};

// Traduce i frame del nodo sensori ESP32 in campioni con provenienza completa.
//
// E' la seconda catena indipendente del sistema: sensori, alimentazione, bus e
// firmware diversi da quelli del Pixhawk. Non e' ridondanza formale, ed e' la
// ragione per cui l'arbitrato di ADR-005 e' per grandezza e il confronto
// incrociato di ADR-004 ha qualcosa da confrontare.
//
// Adapter di sola lettura. Il protocollo prevede un frame di comando verso il
// nodo, e questa classe non lo costruisce: non esiste alcun percorso di
// trasmissione, come per l'adapter MAVLink.
class Esp32CanAdapter final {
public:
    explicit Esp32CanAdapter(NodeId node = 21, NodeHealthPolicy policy = {});

    void consume(std::span<const can::Frame> frames, MonoTime now);
    void consume(const can::Frame& frame, MonoTime now);

    // Applica i timeout. Va chiamata anche quando non arriva nulla.
    void update(MonoTime now);

    [[nodiscard]] const FlightSnapshot& snapshot() const noexcept { return snapshot_; }
    [[nodiscard]] const Esp32SensorState& sensor_state() const noexcept {
        return state_;
    }
    [[nodiscard]] const NodeHealthMonitor& health() const noexcept { return health_; }
    [[nodiscard]] const Esp32AdapterStatistics& statistics() const noexcept {
        return statistics_;
    }
    [[nodiscard]] const std::vector<NodeStateChange>& node_events() const noexcept {
        return node_events_;
    }
    void clear_node_events() noexcept { node_events_.clear(); }

private:
    void apply_attitude(const can::Frame& frame, MonoTime now);
    void apply_air_data(const can::Frame& frame, MonoTime now);
    void apply_health(const can::Frame& frame, MonoTime now);
    void apply_diagnostics(const can::Frame& frame);
    void apply_gps_position(const can::Frame& frame, MonoTime now);
    void apply_gps_velocity(const can::Frame& frame, MonoTime now);
    void apply_gps_status(const can::Frame& frame);
    void revalidate();
    void collect(std::vector<NodeStateChange> changes);

    NodeId node_;
    NodeHealthMonitor health_;
    FlightSnapshot snapshot_{};
    Esp32SensorState state_{};
    Esp32AdapterStatistics statistics_{};
    std::vector<NodeStateChange> node_events_;
    std::uint64_t sequence_{};
    std::uint16_t last_heartbeat_{};
    bool heartbeat_seen_{};
};

}  // namespace efis
