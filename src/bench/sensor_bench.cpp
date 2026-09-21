#include "efis/bench/sensor_bench.hpp"
#include "efis/core/freshness_budget.hpp"

#include <algorithm>
#include <utility>

namespace efis::bench {
namespace {

// Oltre questo silenzio una riga del banco non descrive piu' il presente. E'
// piu' generoso della soglia del modello di volo, perche' qui non si pilota:
// serve a distinguere un valore fermo da uno appena arrivato, non a proteggere
// un comando.
constexpr auto bench_stale_after = FreshnessBudget::bench_stale;

constexpr std::size_t read_chunk = 4096;

BenchRow make_row(const Sample<double>& sample, MonoTime now) {
    BenchRow row{};
    row.signal = sample.origin.signal;
    row.value = sample.value;
    row.raw = sample.raw;
    row.unit = sample.origin.unit;
    row.validity = sample.validity;
    row.source = sample.source;
    row.node = sample.origin.node;
    row.simulated = sample.is_simulated();

    if (sample.origin.signal == SignalId::unknown) {
        row.usable = false;
        row.note = BenchNote::never_received;
        return row;
    }

    row.age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - sample.received_at);

    // L'ordine dei motivi non e' indifferente. Si dichiara la ragione piu'
    // vicina alla causa: un dato scaduto perche' il nodo tace e' un guasto
    // diverso da un dato fresco che il sensore stesso dichiara inattendibile, e
    // mostrare il motivo sbagliato manda a cercare nel posto sbagliato.
    if (!is_usable(sample.validity)) {
        row.usable = false;
        row.note = sample.validity == Validity::stale ? BenchNote::stale
                                                      : BenchNote::never_received;
        return row;
    }
    if (row.age > bench_stale_after) {
        row.usable = false;
        row.note = BenchNote::stale;
        return row;
    }
    if (row.simulated) {
        // Utilizzabile per guardarlo, mai confondibile con una misura.
        row.usable = true;
        row.note = BenchNote::simulated;
        return row;
    }
    row.usable = true;
    row.note = BenchNote::none;
    return row;
}

void add(std::vector<BenchRow>& rows, const Sample<double>& sample, MonoTime now) {
    // Un segnale mai ricevuto non ha una riga: il banco elenca cio' che il
    // Pixhawk trasmette davvero. Una riga vuota per ogni grandezza immaginabile
    // riempirebbe lo schermo di caselle spente e nasconderebbe quelle che
    // contano.
    if (sample.origin.signal == SignalId::unknown) return;
    rows.push_back(make_row(sample, now));
}

}  // namespace

const BenchRow* BenchView::find(SignalId signal) const noexcept {
    const auto it = std::find_if(rows.begin(), rows.end(),
                                 [signal](const BenchRow& row) {
                                     return row.signal == signal;
                                 });
    return it == rows.end() ? nullptr : &*it;
}

SensorBench::SensorBench(std::unique_ptr<io::ByteSource> source,
                         NodeHealthPolicy policy)
    : source_(std::move(source)), adapter_(policy), buffer_(read_chunk) {}

bool SensorBench::exhausted() const noexcept {
    return source_ == nullptr || source_->exhausted();
}

void SensorBench::pump(MonoTime now) {
    if (source_ != nullptr) {
        const std::size_t read = source_->read(std::span<std::uint8_t>{buffer_});
        if (read > 0) {
            adapter_.consume(std::span<const std::uint8_t>{buffer_.data(), read}, now);
        }
    }
    // Anche senza byte nuovi lo stato va aggiornato: i timeout dei nodi
    // scattano per il tempo che passa, non per i dati che arrivano.
    adapter_.update(now);
    rebuild(now);
}

void SensorBench::rebuild(MonoTime now) {
    const FlightSnapshot& snapshot = adapter_.snapshot();
    const AirDataSensorState& air = adapter_.air_data_sensor();

    BenchView view{};
    view.rows.reserve(12);

    add(view.rows, snapshot.attitude.roll_deg, now);
    add(view.rows, snapshot.attitude.pitch_deg, now);
    add(view.rows, snapshot.attitude.heading_deg, now);
    add(view.rows, snapshot.air_data.pressure_altitude_ft, now);
    add(view.rows, snapshot.air_data.vertical_speed_fpm, now);

    // Il sensore aria in tre righe distinte, e non e' ridondanza. La pressione
    // differenziale e la temperatura dell'elemento sono cio' che il sensore
    // misura; la velocita' e' cio' che l'autopilota ne deduce. Confonderle
    // significherebbe non poter dire, davanti a una velocita' sbagliata, se a
    // sbagliare sia il sensore o il calcolo.
    add(view.rows, air.differential_pressure_hpa, now);
    add(view.rows, air.element_temperature_c, now);
    add(view.rows, air.computed_airspeed_kt, now);

    add(view.rows, snapshot.position.latitude_deg, now);
    add(view.rows, snapshot.position.longitude_deg, now);
    add(view.rows, snapshot.position.ground_speed_kt, now);

    view.gnss_fix_type = adapter_.gnss_fix_type();
    view.gnss_satellites = adapter_.gnss_satellites();
    view.air_data_present = air.declared_present;
    view.air_data_healthy = air.declared_healthy;

    // Le regole di validita' sono gia' applicate dall'adapter. Qui si dichiara
    // soltanto il motivo, perche' una riga spenta senza spiegazione manda a
    // cercare il guasto nel posto sbagliato.
    for (BenchRow& row : view.rows) {
        if (row.usable) continue;
        if (row.note != BenchNote::never_received) continue;
        const bool positional = row.signal == SignalId::latitude ||
                                row.signal == SignalId::longitude ||
                                row.signal == SignalId::ground_speed;
        if (positional && view.gnss_fix_type < gnss_fix_3d) {
            row.note = BenchNote::no_gnss_fix;
        } else if (row.signal == SignalId::indicated_airspeed &&
                   view.air_data_present && !view.air_data_healthy) {
            row.note = BenchNote::sensor_declared_unhealthy;
        }
    }

    view.nodes = adapter_.health().nodes();

    const auto& parser = adapter_.parser_statistics();
    view.frames = parser.frames;
    view.crc_errors = parser.crc_errors;
    view.unknown_messages = parser.unknown_messages;
    view.dropped_bytes = parser.dropped_bytes;
    view.status_text = adapter_.last_status_text();

    view_ = std::move(view);
}

}  // namespace efis::bench
