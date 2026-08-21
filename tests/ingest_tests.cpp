// Prove della catena di ingestione: dai byte del Pixhawk allo stato protetto.
//
// Non verificano che arbitrato, stasi e confronto incrociato funzionino: quello
// lo fanno gia' i test dei singoli moduli. Verificano che siano **raggiunti**
// partendo dai byte, che e' cosa diversa e per un certo tempo non era vera.

#include "efis/ingest/mavlink_pipeline.hpp"
#include "efis/ingest/recording_session.hpp"
#include "efis/io/byte_source.hpp"
#include "efis/recording/flight_record.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// Costruttore di frame MAVLink 2. Il checksum e' quello che il protocollo
// impone: se il CRC_EXTRA della tabella fosse sbagliato questi frame verrebbero
// rifiutati e le prove fallirebbero, invece di passare su dati inventati.
std::vector<std::uint8_t> frame_v2(std::uint32_t id, std::uint8_t crc_extra,
                                   const std::vector<std::uint8_t>& payload,
                                   std::uint8_t sequence) {
    std::vector<std::uint8_t> body{
        static_cast<std::uint8_t>(payload.size()), 0, 0, sequence, 1, 1,
        static_cast<std::uint8_t>(id & 0xFF),
        static_cast<std::uint8_t>((id >> 8) & 0xFF),
        static_cast<std::uint8_t>((id >> 16) & 0xFF)};
    body.insert(body.end(), payload.begin(), payload.end());

    std::uint16_t crc = 0xFFFF;
    auto accumulate = [&crc](std::uint8_t byte) {
        auto tmp = static_cast<std::uint8_t>(byte ^ static_cast<std::uint8_t>(crc & 0xFF));
        tmp = static_cast<std::uint8_t>(tmp ^ static_cast<std::uint8_t>(tmp << 4));
        crc = static_cast<std::uint16_t>((crc >> 8) ^ (static_cast<std::uint16_t>(tmp) << 8) ^
                                         (static_cast<std::uint16_t>(tmp) << 3) ^
                                         (static_cast<std::uint16_t>(tmp) >> 4));
    };
    for (std::uint8_t byte : body) accumulate(byte);
    accumulate(crc_extra);

    std::vector<std::uint8_t> out{0xFD};
    out.insert(out.end(), body.begin(), body.end());
    out.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    out.push_back(static_cast<std::uint8_t>(crc >> 8));
    return out;
}

void put_f32(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t bits{};
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((bits >> shift) & 0xFF));
    }
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

constexpr double meters_per_second_per_knot = 1.0 / 1.943844;

// VFR_HUD (74). Velocita' in metri al secondo, come vuole il protocollo: la
// conversione in nodi e' compito dell'adapter, e passare qui gia' dei nodi
// renderebbe la prova cieca proprio su quella conversione.
std::vector<std::uint8_t> vfr_hud(double airspeed_kt, double ground_speed_kt,
                                  double altitude_ft, std::uint8_t sequence) {
    std::vector<std::uint8_t> payload;
    put_f32(payload, static_cast<float>(airspeed_kt * meters_per_second_per_knot));
    put_f32(payload, static_cast<float>(ground_speed_kt * meters_per_second_per_knot));
    put_f32(payload, static_cast<float>(altitude_ft * 0.3048));
    put_f32(payload, 0.0F);                       // climb
    payload.push_back(0); payload.push_back(0);   // heading
    payload.push_back(0); payload.push_back(0);   // throttle
    return frame_v2(74, 20, payload, sequence);
}

// SYS_STATUS (1). Il bit del sensore di pressione differenziale governa la
// validita' della velocita': senza un SYS_STATUS che lo dichiari sano l'adapter
// non presenta la velocita' come valida, ed e' il comportamento corretto.
constexpr std::uint32_t sensor_differential_pressure = 0x20;

std::vector<std::uint8_t> sys_status(bool differential_pressure_healthy,
                                     std::uint8_t sequence) {
    const std::uint32_t present = sensor_differential_pressure;
    const std::uint32_t healthy = differential_pressure_healthy
                                      ? sensor_differential_pressure
                                      : 0U;
    std::vector<std::uint8_t> payload;
    put_u32(payload, present);
    put_u32(payload, present);   // enabled
    put_u32(payload, healthy);
    payload.resize(31, 0);
    return frame_v2(1, 124, payload, sequence);
}

// GPS_RAW_INT (24). La velocita' al suolo non e' utilizzabile senza un fix
// tridimensionale, e l'adapter la invalida di conseguenza: e' corretto, perche'
// senza fix quel numero non descrive niente. Le prove che dipendono dal moto
// devono quindi dichiarare un fix vero, come farebbe l'aeromobile.
std::vector<std::uint8_t> gps_fix_3d(std::uint8_t sequence) {
    std::vector<std::uint8_t> payload(30, 0);
    payload[28] = 3;    // fix_type: tridimensionale
    payload[29] = 14;   // satelliti in vista
    return frame_v2(24, 24, payload, sequence);
}

void put_i16(efis::can::Frame& frame, std::uint8_t offset, int value) {
    const auto raw = static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
    frame.data[offset] = static_cast<std::uint8_t>(raw & 0xFFU);
    frame.data[offset + 1] = static_cast<std::uint8_t>((raw >> 8U) & 0xFFU);
}

efis::can::Frame esp32_attitude(bool imu_ok, bool barometer_ok, bool pitot_ok) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_attitude;
    frame.length = 8;
    put_i16(frame, 0, 0);
    put_i16(frame, 2, 0);
    put_i16(frame, 4, 9000);
    frame.data[6] = static_cast<std::uint8_t>((imu_ok ? 1U : 0U) |
                                              (barometer_ok ? 2U : 0U) |
                                              (pitot_ok ? 4U : 0U));
    frame.data[7] = static_cast<std::uint8_t>(3U | (3U << 2U) |
                                              (3U << 4U) | (3U << 6U));
    return frame;
}

efis::can::Frame esp32_air_data(double airspeed_kt, int altitude_ft) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_air_data;
    frame.length = 8;
    put_i16(frame, 0, static_cast<int>(airspeed_kt * 10.0));
    put_i16(frame, 2, altitude_ft);
    put_i16(frame, 4, 132);  // 1013,2 hPa con offset wire di 1000 hPa.
    put_i16(frame, 6, 475);  // 47,5 Pa.
    return frame;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};

    // --- La catena e' percorsa fino in fondo ---------------------------------
    //
    // Un solo frame deve arrivare dallo stato grezzo dell'adapter fino allo
    // stato normalizzato, passando per l'arbitro. Se l'arbitro non fosse nel
    // percorso, `selected_for` non saprebbe rispondere.
    {
        efis::ingest::MavlinkPipeline pipeline;
        const auto health = sys_status(true, 1);
        const auto hud = vfr_hud(120.0, 118.0, 3000.0, 2);
        pipeline.consume(health, epoch + 1s);
        pipeline.consume(hud, epoch + 1s);

        const auto& state = pipeline.state();
        require(state.air_data.indicated_airspeed_kt.usable(),
                "a healthy differential pressure sensor must yield a usable airspeed");
        require(std::abs(state.air_data.indicated_airspeed_kt.value - 120.0) < 0.5,
                "the airspeed must reach the normalized state, converted to knots");
        require(state.air_data.indicated_airspeed_kt.origin.signal ==
                    efis::SignalId::indicated_airspeed,
                "provenance must survive arbitration and normalization");
        require(pipeline.selected_for(efis::SignalId::indicated_airspeed) ==
                    efis::SourceId::pixhawk_primary,
                "the arbitrator must be in the path and name the chosen source");
        require(pipeline.arbitrator().statistics().mislabelled_samples == 0,
                "the adapter must not put a quantity in the wrong field");
    }

    // --- Due bus entrano davvero nello stesso arbitro -----------------------
    //
    // Il Pixhawk e' preferito. Quando dichiara non sano il proprio sensore di
    // pressione differenziale, il solo segnale IAS passa immediatamente alla
    // catena ESP32; quota e assetto non devono essere trascinati dal guasto.
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(true, 1), epoch + 1s);
        pipeline.consume(vfr_hud(120.0, 118.0, 3000.0, 2), epoch + 1s);
        pipeline.consume_can(esp32_attitude(true, true, true), epoch + 1s);
        pipeline.consume_can(esp32_air_data(87.4, 3200), epoch + 1s);

        require(pipeline.selected_for(efis::SignalId::indicated_airspeed) ==
                    efis::SourceId::pixhawk_primary,
                "the preferred Pixhawk airspeed must win while healthy");
        require(std::abs(pipeline.state().air_data.indicated_airspeed_kt.value -
                         120.0) < 0.5,
                "the selected value must be the Pixhawk value before failure");

        pipeline.consume(sys_status(false, 3), epoch + 1100ms);
        require(pipeline.selected_for(efis::SignalId::indicated_airspeed) ==
                    efis::SourceId::esp32_legacy,
                "an unhealthy Pixhawk pitot must fail over immediately to ESP32");
        require(std::abs(pipeline.state().air_data.indicated_airspeed_kt.value -
                         87.4) < 0.05,
                "the protected state must carry the ESP32 value after failover");
        require(pipeline.arbitrator().statistics().failovers >= 1,
                "the cross-bus failover must be counted for the black box");

        bool airspeed_change_recorded = false;
        for (const auto& event : pipeline.source_events()) {
            if (event.signal == efis::SignalId::indicated_airspeed &&
                event.selected == efis::SourceId::esp32_legacy) {
                airspeed_change_recorded = true;
            }
        }
        require(airspeed_change_recorded,
                "the black box event stream must name the ESP32 failover");
    }

    // --- Il sensore dichiarato non sano non produce una velocita' valida -----
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(false, 1), epoch + 1s);
        pipeline.consume(vfr_hud(120.0, 118.0, 3000.0, 2), epoch + 1s);
        require(!pipeline.state().air_data.indicated_airspeed_kt.usable(),
                "an unhealthy sensor must not produce a usable value");
    }

    // --- Il valore fermo viene visto ----------------------------------------
    //
    // Velocita' indicata bloccata su 90 nodi mentre quota e velocita' al suolo
    // continuano a cambiare: il pitot ostruito. Nessun controllo sul flusso lo
    // rileva, perche' i frame arrivano regolari e con sequenza crescente.
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(true, 0), epoch);
        pipeline.consume(gps_fix_3d(1), epoch);

        for (int tick = 1; tick <= 25; ++tick) {
            const auto now = epoch + std::chrono::seconds{tick};
            // La velocita' al suolo resta sopra la soglia di moto e cambia a
            // ogni passo, cosi' il cancello della sorveglianza e' aperto e non e'
            // lei la grandezza ferma.
            pipeline.consume(vfr_hud(90.0, 95.0 + tick * 0.7, 3000.0 + tick * 20.0,
                                     static_cast<std::uint8_t>(tick)), now);
        }

        const auto& health = pipeline.state().health;
        require(health.suspect_fields > 0,
                "a value that stops changing while airborne must be reported as suspect");
        require(health.frozen_signal == efis::SignalId::indicated_airspeed,
                "the frozen quantity must be named, and it is the airspeed");
        require(pipeline.model().stasis().motion(efis::SignalId::indicated_airspeed) ==
                    efis::Motion::suspect,
                "the stasis monitor must name the airspeed as the stuck quantity");
    }

    // --- Fermi a terra la sorveglianza resta chiusa --------------------------
    //
    // Stessa velocita' costante, ma senza moto: a terra e' legittima, e un
    // allarme a ogni sosta sarebbe un allarme che nessuno guarda piu'.
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(true, 0), epoch);
        pipeline.consume(gps_fix_3d(1), epoch);
        for (int tick = 1; tick <= 25; ++tick) {
            pipeline.consume(vfr_hud(0.0, 2.0, 590.0, static_cast<std::uint8_t>(tick)),
                             epoch + std::chrono::seconds{tick});
        }
        require(pipeline.state().health.suspect_fields == 0,
                "on the ground a constant value must not raise a stasis alarm");
    }

    // --- Il disaccordo fra catene indipendenti viene visto -------------------
    //
    // Velocita' indicata quasi nulla mentre quella al suolo e' di 130 nodi. Una
    // sola sorgente sbaglierebbe in modo coerente e resterebbe invisibile; il
    // confronto la contraddice.
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(true, 0), epoch);
        pipeline.consume(gps_fix_3d(1), epoch);
        for (int tick = 1; tick <= 12; ++tick) {
            pipeline.consume(vfr_hud(3.0, 130.0 + tick * 0.8, 3000.0 + tick * 25.0,
                                     static_cast<std::uint8_t>(tick)),
                             epoch + std::chrono::seconds{tick});
        }

        const auto& state = pipeline.state();
        require(state.health.disagreeing_fields > 0,
                "two independent chains that contradict each other must be reported");
        bool announced = false;
        for (const auto& alert : state.alerts.active) {
            if (alert.code == "DATA_DISAGREE") announced = true;
        }
        require(announced, "the disagreement must reach the alert list, not only the counters");
    }

    // --- Il silenzio produce comunque un effetto -----------------------------
    //
    // Scadenza, stasi e disaccordo si misurano nel tempo: senza update() un
    // collegamento che ammutolisce resterebbe all'ultimo stato buono, che e' il
    // modo peggiore di fallire.
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(true, 1), epoch + 1s);
        pipeline.consume(vfr_hud(120.0, 118.0, 3000.0, 2), epoch + 1s);
        require(pipeline.state().health.stale_fields == 0,
                "fresh data must not be stale");

        pipeline.update(epoch + 30s);
        require(pipeline.state().health.stale_fields > 0,
                "silence must age the data even though no byte arrived");
        require(!pipeline.state().air_data.indicated_airspeed_kt.usable(),
                "a value nobody has refreshed must stop being usable");

        bool lost = false;
        for (const auto& event : pipeline.node_events()) {
            if (event.current == efis::NodeState::lost) lost = true;
        }
        require(lost, "prolonged silence must be recorded as the loss of the node");
    }

    // --- Gli eventi si svuotano insieme --------------------------------------
    {
        efis::ingest::MavlinkPipeline pipeline;
        pipeline.consume(sys_status(true, 1), epoch + 1s);
        pipeline.update(epoch + 30s);
        require(!pipeline.node_events().empty(), "there must be something to clear");
        pipeline.clear_events();
        require(pipeline.node_events().empty() && pipeline.source_events().empty(),
                "clearing must empty both the node and the source event queues");
    }


    // --- Dalla registrazione reale alla black box ----------------------------
    //
    // La prova che conta per questo passo: i byte veri di ArduPlane 4.7.0
    // attraversano la catena, arrivano nel registro a segmenti e si rileggono.
    // Prima di questo innesto la black box conservava campioni del simulatore
    // mentre il Pixhawk parlava a vuoto.
    {
        const auto directory = std::filesystem::temp_directory_path() /
            "efis-ingest-replay";
        std::filesystem::remove_all(directory);

        efis::io::RecordedByteSource source{
            std::filesystem::path{EFIS_TEST_DATA_DIR} / "mavlink" /
            "arduplane-4.7.0-usb-20260821.mav"};
        require(source.size() > 0, "the reference capture must be readable");

        efis::ingest::RecordingSessionStatistics statistics{};
        {
            efis::recording::SegmentedRecorder recorder{
                directory, {.records_per_segment = 100000, .flush_every_records = 1}};
            efis::ingest::RecordingSession session{recorder};

            int step = 0;
            while (!source.exhausted() && step < 400) {
                session.step(source, epoch + std::chrono::milliseconds{20 * ++step});
            }
            statistics = session.statistics();

            require(session.pipeline().adapter().statistics().frames_applied > 0,
                    "the real capture must yield frames the adapter understands");
            recorder.close();
        }

        require(statistics.bytes_read == source.size(),
                "the whole capture must be consumed, not a prefix of it");
        require(statistics.snapshots_recorded > 0, "snapshots must be recorded");

        std::vector<std::filesystem::path> segments;
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (entry.path().extension() == ".csv") segments.push_back(entry.path());
        }
        require(segments.size() == 1, "the recorder must have closed one segment");

        efis::recording::FlightRecord record{};
        {
            std::ifstream file{segments.front(), std::ios::binary};
            record = efis::recording::read_flight_record(file);
        }
        std::filesystem::remove_all(directory);

        require(record.schema == 2, "the segment must declare its schema");
        require(record.malformed_rows == 0,
                "a segment closed cleanly must contain no unreadable row");
        require(!record.samples.empty(), "the segment must contain samples");

        // La provenienza deve sopravvivere al giro completo: se si perdesse, la
        // registrazione sarebbe una colonna di numeri senza significato.
        bool real_airspeed = false;
        for (const auto& entry : record.samples) {
            if (entry.sample.origin.signal != efis::SignalId::indicated_airspeed) continue;
            if (entry.sample.origin.unit != efis::Unit::knot) continue;
            if (entry.sample.is_simulated()) continue;
            real_airspeed = true;
        }
        require(real_airspeed,
                "the recorded airspeed must come from the capture, not from the simulator");

        // Gli eventi devono essere nel registro, non solo nei contatori.
        require(statistics.source_events_recorded > 0,
                "choosing a source for the first time is an event and must be recorded");
        require(!record.source_changes.empty(),
                "the source change must be re-readable from the segment");
    }

    // --- Il silenzio finisce nel registro ------------------------------------
    //
    // Un collegamento che ammutolisce non produce byte, ma produce storia: il
    // nodo invecchia e viene dichiarato perduto. Se quella transizione non
    // arrivasse nel registro, rileggendo il volo non si potrebbe distinguere un
    // controllore che ha smesso di parlare da uno che non ha mai parlato.
    {
        const auto directory = std::filesystem::temp_directory_path() /
            "efis-ingest-silence";
        std::filesystem::remove_all(directory);

        efis::io::RecordedByteSource source{
            std::vector<std::uint8_t>{sys_status(true, 1)}};

        efis::ingest::RecordingSessionStatistics statistics{};
        {
            efis::recording::SegmentedRecorder recorder{
                directory, {.records_per_segment = 100000, .flush_every_records = 1}};
            efis::ingest::RecordingSession session{recorder};

            session.step(source, epoch + 1s);
            require(session.statistics().bytes_read > 0, "the first step must read data");

            // Poi piu' niente, per un tempo superiore alla perdita del nodo.
            for (int tick = 2; tick <= 12; ++tick) {
                session.step(source, epoch + std::chrono::seconds{tick});
            }
            statistics = session.statistics();
            recorder.close();
        }

        require(statistics.silent_steps >= 10,
                "steps without data must be counted as silent, not skipped");
        require(statistics.node_events_recorded > 0,
                "losing the node is an event and must be recorded");

        std::vector<std::filesystem::path> segments;
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (entry.path().extension() == ".csv") segments.push_back(entry.path());
        }
        require(segments.size() == 1, "one segment must have been closed");

        efis::recording::FlightRecord record{};
        {
            std::ifstream file{segments.front(), std::ios::binary};
            record = efis::recording::read_flight_record(file);
        }
        std::filesystem::remove_all(directory);

        bool lost = false;
        for (const auto& event : record.node_events) {
            if (event.current == efis::NodeState::lost) lost = true;
        }
        require(lost, "the loss of the node must be re-readable from the segment");

        // Un evento gia' scritto non deve tornare al passo successivo: una
        // registrazione che ripete cento volte la stessa perdita non e' piu'
        // leggibile.
        std::size_t losses = 0;
        for (const auto& event : record.node_events) {
            if (event.current == efis::NodeState::lost) ++losses;
        }
        require(losses == 1, "the same transition must be recorded once, not at every step");
    }

    std::cout << "All EFIS ingest tests passed\n";
}
