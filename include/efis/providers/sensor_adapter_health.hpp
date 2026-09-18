#pragma once

#include "efis/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace efis {

// Stato fail-closed di un singolo canale del futuro adapter ASSO5. Il
// componente non conosce I2C, Sequent o il trasporto di rete: trasforma soltanto
// osservazioni gia' acquisite in uno stato affidabile e quindi resta testabile
// prima che l'hardware reale sia inventariato.
enum class SensorChannelState : std::uint8_t {
    unknown,
    valid,
    stale,
    lost,
    failed,
    simulated
};

enum class SensorAdapterMode : std::uint8_t { real, simulation };

enum class SensorFault : std::uint8_t {
    none,
    board_absent,
    bus_timeout,
    open_input,
    short_input,
    invalid_range,
    clock_regression,
    adapter_restart
};

enum class SensorObservationVerdict : std::uint8_t {
    accepted,
    accepted_after_gap,
    accepted_recovery_pending,
    fault_reported,
    rejected_unconfigured,
    rejected_duplicate,
    rejected_regressed_sequence,
    rejected_regressed_time,
    rejected_mode_mismatch
};

struct SensorChannelConfig final {
    SignalId signal{SignalId::unknown};
    NodeId node{node_undeclared};
    ChannelId channel{channel_undeclared};
    Unit unit{Unit::unknown};
    SensorAdapterMode mode{SensorAdapterMode::real};
    std::uint32_t calibration_revision{};
};

struct SensorChannelPolicy final {
    // Sono parametri iniettati, non valori hardware congelati. Le soglie reali
    // verranno decise soltanto dopo l'inventario G1.
    std::chrono::milliseconds stale_after{};
    std::chrono::milliseconds lost_after{};
    std::uint32_t recovery_samples{};
};

struct SensorChannelStatus final {
    SensorChannelState state{SensorChannelState::unknown};
    SensorFault last_fault{SensorFault::none};
    std::optional<MonoTime> last_observation{};
    std::optional<std::uint64_t> last_sequence{};
    std::uint64_t accepted{};
    std::uint64_t duplicates{};
    std::uint64_t regressions{};
    std::uint64_t time_regressions{};
    std::uint64_t mode_mismatches{};
    std::uint64_t missed{};
    std::uint64_t fault_events{};
    std::uint64_t restarts{};
    std::uint32_t recovery_progress{};
    bool last_observation_simulated{};
};

[[nodiscard]] constexpr bool sensor_channel_usable(SensorChannelState state) noexcept {
    return state == SensorChannelState::valid ||
           state == SensorChannelState::simulated;
}

[[nodiscard]] constexpr Validity sensor_channel_validity(
    SensorChannelState state) noexcept {
    switch (state) {
        case SensorChannelState::valid: return Validity::valid;
        case SensorChannelState::simulated: return Validity::simulated;
        case SensorChannelState::stale: return Validity::stale;
        case SensorChannelState::unknown:
        case SensorChannelState::lost:
        case SensorChannelState::failed: return Validity::unavailable;
    }
    return Validity::unavailable;
}

class SensorAdapterHealth final {
public:
    SensorAdapterHealth() = default;

    // Una configurazione incompleta o una policy temporale incoerente viene
    // rifiutata. Nessun default implicito puo' trasformarsi in dato reale.
    [[nodiscard]] bool configure(SensorChannelConfig config,
                                 SensorChannelPolicy policy) noexcept;

    [[nodiscard]] bool configured() const noexcept { return configured_; }
    [[nodiscard]] const SensorChannelConfig& config() const noexcept { return config_; }
    [[nodiscard]] const SensorChannelStatus& status() const noexcept { return status_; }

    // `simulated` deve concordare con la modalita' configurata. Un fault e'
    // un'osservazione diagnostica, non un campione valido, e non aggiorna il
    // tempo dell'ultimo dato buono.
    [[nodiscard]] SensorObservationVerdict observe(
        MonoTime at, std::uint64_t sequence, bool simulated,
        SensorFault fault = SensorFault::none) noexcept;

    // Applica STALE/LOST usando esclusivamente il clock monotono del consumer.
    [[nodiscard]] SensorChannelState update(MonoTime now) noexcept;

    // Un riavvio annulla la freshness e richiede una nuova sequenza; non
    // conserva mai un dato precedente come corrente.
    void restart(MonoTime at) noexcept;

private:
    [[nodiscard]] SensorChannelState nominal_state() const noexcept;

    SensorChannelConfig config_{};
    SensorChannelPolicy policy_{};
    SensorChannelStatus status_{};
    bool configured_{};
};

}  // namespace efis
