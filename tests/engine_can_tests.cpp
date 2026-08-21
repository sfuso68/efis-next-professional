// Prove dell'adapter del bus motore, Engine CAN V1.
//
// Non serve hardware: i frame sono scritti secondo il protocollo, e lo stesso
// formato candump si rigioca dal nodo black box con vcan0. Se il protocollo e
// l'adapter divergessero, queste prove non se ne accorgerebbero da sole: per
// questo il formato delle righe e' quello reale di candump e non un'invenzione.

#include "efis/io/can_source.hpp"
#include "efis/providers/engine_can_adapter.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close_to(double value, double expected) {
    return std::abs(value - expected) < 1e-6;
}

// Compone una riga candump come la produce `candump -L`.
std::string line(std::uint16_t id, const std::vector<std::uint8_t>& payload) {
    static const char* digits = "0123456789ABCDEF";
    std::string text = "(1755772800.123456) vcan0 ";
    for (int shift = 8; shift >= 0; shift -= 4) {
        text.push_back(digits[(id >> shift) & 0x0F]);
    }
    text.push_back('#');
    for (const std::uint8_t byte : payload) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0F]);
    }
    return text;
}

constexpr std::uint16_t frame_id(std::uint8_t node, std::uint8_t message) {
    return static_cast<std::uint16_t>((node << 7) | message);
}

std::vector<std::uint8_t> u16le(std::uint16_t value) {
    return {static_cast<std::uint8_t>(value & 0xFF),
            static_cast<std::uint8_t>(value >> 8)};
}

std::vector<std::uint8_t> concat(std::vector<std::vector<std::uint8_t>> parts) {
    std::vector<std::uint8_t> out;
    for (const auto& part : parts) out.insert(out.end(), part.begin(), part.end());
    return out;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};
    const auto now = epoch + 1s;
    constexpr std::uint8_t engine_node = 1;

    // --- Lettura del formato candump -----------------------------------------
    const auto parsed = efis::io::parse_candump_line(
        "(1755772800.123456) vcan0 081#0C6009B60302");
    require(parsed.has_value(), "a well formed candump line must be read");
    require(parsed->id == 0x081, "the identifier must be read in hexadecimal");
    require(parsed->node() == 1, "the node must be the four high bits");
    require(parsed->message() == 0x01, "the message must be the seven low bits");
    require(parsed->length == 6, "the payload length follows from the digits");

    require(!efis::io::parse_candump_line("questa non e' una riga").has_value(),
            "a line without a frame must be refused, not guessed");
    require(!efis::io::parse_candump_line("(1) vcan0 081#0C600").has_value(),
            "an odd number of digits cannot be a payload");
    require(!efis::io::parse_candump_line("(1) vcan0 081#R").has_value(),
            "a remote frame does not belong to Engine CAN V1");
    require(!efis::io::parse_candump_line("(1) vcan0 800#00").has_value(),
            "an identifier beyond eleven bits must be refused");

    // --- Misure primarie ------------------------------------------------------
    //
    // I fattori di scala vivono nel protocollo, non nel codice che li usa: qui si
    // verifica che l'adapter li applichi come documentato, e che il valore
    // trasmesso resti conservato come grezzo.
    efis::EngineCanAdapter adapter{{.stale_after = 1000ms, .lost_after = 3000ms}};
    std::vector<std::string> capture{
        // seq=0, rpm=2400, manifold=23.70 inHg, oil=72.0 psi
        line(frame_id(engine_node, efis::engine_can::engine_primary),
             concat({{0}, u16le(2400), u16le(2370), u16le(720)})),
        // seq=0, oil_temp=94.0 C, fuel_press=5.20 psi, fuel_flow=9.60 gph
        line(frame_id(engine_node, efis::engine_can::engine_fluids),
             concat({{0}, u16le(940), u16le(520), u16le(960)})),
        // seq=0, left=18.00 gal, right=17.50 gal
        line(frame_id(engine_node, efis::engine_can::engine_fuel),
             concat({{0}, u16le(1800), u16le(1750)})),
        // seq=0, bus=14.10 V, alternator=18.00 A
        line(frame_id(engine_node, efis::engine_can::engine_electrical),
             concat({{0}, u16le(1410), u16le(1800)})),
        // seq=0, cht1=178.0 C, cht2=181.0 C
        line(frame_id(engine_node, efis::engine_can::cht_1_2),
             concat({{0}, u16le(1780), u16le(1810)})),
        // seq=0, egt3=742.0 C, egt4=748.0 C
        line(frame_id(engine_node, efis::engine_can::egt_3_4),
             concat({{0}, u16le(7420), u16le(7480)})),
        // battito: uptime 3600 s, taratura valida
        line(frame_id(engine_node, efis::engine_can::node_heartbeat),
             concat({{0}, {0x10, 0x0E, 0x00, 0x00},
                     {efis::engine_can::flag_calibration_valid}})),
    };

    efis::io::RecordedCanSource source{capture};
    adapter.consume(source, now);

    require(adapter.statistics().frames_applied == 7, "every frame must be applied");
    require(adapter.statistics().frames_wrong_length == 0,
            "every frame carried exactly its declared length");
    require(adapter.statistics().frames_ignored == 0, "every message is in the table");

    const auto& engine = adapter.snapshot();
    require(engine.rpm.value == 2400.0, "the engine speed must decode");
    require(engine.rpm.raw == 2400.0, "with a unit scale raw and value coincide");
    require(engine.rpm.origin.unit == efis::Unit::revolutions_per_minute,
            "the engine speed must declare its unit");
    require(close_to(engine.manifold_pressure_inhg.value, 23.70),
            "the manifold pressure must be scaled by one hundredth");
    require(engine.manifold_pressure_inhg.raw == 2370.0,
            "the raw value must keep what the bus carried");
    require(close_to(engine.oil_pressure_psi.value, 72.0),
            "the oil pressure must be scaled by one tenth");
    require(close_to(engine.oil_temperature_c.value, 94.0),
            "the oil temperature must decode");
    require(close_to(engine.fuel_flow_gph.value, 9.60),
            "the fuel flow must decode");
    require(close_to(engine.fuel_left_gal.value, 18.0) &&
                close_to(engine.fuel_right_gal.value, 17.50),
            "the two tanks must decode separately");
    require(close_to(engine.bus_voltage_v.value, 14.10),
            "the bus voltage must decode");
    require(close_to(engine.alternator_current_a.value, 18.0),
            "the alternator current must decode");
    require(close_to(engine.cht_c[0].value, 178.0) &&
                close_to(engine.cht_c[1].value, 181.0),
            "the first two cylinder heads must decode");
    require(engine.cht_c[0].origin.signal ==
                efis::SignalId::cylinder_head_temperature_1,
            "each cylinder must carry its own signal");
    require(close_to(engine.egt_c[2].value, 742.0) &&
                close_to(engine.egt_c[3].value, 748.0),
            "the second exhaust frame must fill cylinders three and four");

    // Cio' che nessun frame ha portato non viene inventato.
    require(!engine.cht_c[2].usable(),
            "a cylinder never transmitted must not become a reading");
    require(engine.cht_c[2].origin.signal == efis::SignalId::unknown,
            "a quantity never received declares no signal");

    // Provenienza: nodo, canale e sorgente.
    require(engine.rpm.origin.node == engine_node,
            "a sample must declare which node produced it");
    require(engine.rpm.origin.channel == efis::engine_can::engine_primary,
            "a sample must declare which message carried it");
    require(engine.rpm.source == efis::SourceId::esp32_legacy,
            "the engine node must be declared as the source");
    require(!engine.rpm.is_simulated(), "bus data is not simulated");

    // Stato che il nodo dichiara di se stesso.
    require(adapter.node_status().seen, "the heartbeat must be recorded");
    require(adapter.node_status().uptime_s == 3600,
            "the uptime must decode as a thirty two bit value");
    require(adapter.node_status().calibration_valid,
            "the calibration flag must decode");
    require(!adapter.node_status().probe_failure,
            "no probe failure was declared in this capture");

    // --- Sonda illeggibile ----------------------------------------------------
    //
    // Il punto che conta di piu'. Una sonda scollegata non e' una pressione a
    // zero: una pressione dell'olio a zero e' un allarme, una sonda scollegata e'
    // un'altra cosa e richiede un'altra azione. L'ADR-004 vieta di confonderle.
    efis::EngineCanAdapter broken{};
    std::vector<std::string> with_failure{
        line(frame_id(engine_node, efis::engine_can::engine_primary),
             concat({{1}, u16le(2400), u16le(2370),
                     u16le(efis::engine_can::unsigned_unavailable)})),
        line(frame_id(engine_node, efis::engine_can::engine_fluids),
             concat({{1}, u16le(static_cast<std::uint16_t>(0x8000)), u16le(520),
                     u16le(960)})),
    };
    efis::io::RecordedCanSource failing{with_failure};
    broken.consume(failing, now);

    require(broken.snapshot().rpm.usable(),
            "a healthy quantity in the same frame must stay usable");
    require(!broken.snapshot().oil_pressure_psi.usable(),
            "a probe declared unreadable must not produce a usable value");
    require(broken.snapshot().oil_pressure_psi.validity ==
                efis::Validity::unavailable,
            "an unreadable probe must be unavailable, never a plausible zero");
    require(!broken.snapshot().oil_temperature_c.usable(),
            "the signed unavailable marker must be honoured too");
    require(broken.statistics().unavailable_readings == 2,
            "unreadable probes must be counted, not silently dropped");

    // --- Frame corto e nodo non dichiarato ------------------------------------
    efis::EngineCanAdapter strict{};
    std::vector<std::string> malformed{
        // ENGINE_PRIMARY vuole sette byte, questo ne porta quattro.
        line(frame_id(engine_node, efis::engine_can::engine_primary),
             concat({{0}, u16le(2400), {0x00}})),
        // Nodo zero: non dichiarato, non accolto.
        line(frame_id(0, efis::engine_can::engine_primary),
             concat({{0}, u16le(2400), u16le(2370), u16le(720)})),
        // Messaggio che non ci riguarda.
        line(frame_id(engine_node, 0x42), concat({{0}, u16le(1)})),
    };
    efis::io::RecordedCanSource odd{malformed};
    strict.consume(odd, now);

    require(strict.statistics().frames_wrong_length == 1,
            "a frame shorter than its message must be refused");
    require(strict.statistics().frames_undeclared_node == 1,
            "node zero means undeclared and must never be accepted");
    require(strict.statistics().frames_ignored == 1,
            "a message outside the table is ignored, not an error");
    require(strict.statistics().frames_applied == 0,
            "none of those frames may reach the snapshot");
    require(!strict.snapshot().rpm.usable(),
            "a refused frame must leave the quantity untouched");

    // --- Salute del nodo e giro del contatore ---------------------------------
    //
    // Ogni messaggio ha un contatore proprio, perche' i periodi sono diversi.
    // Tenerne uno solo per nodo produrrebbe una regressione a ogni frame.
    efis::EngineCanAdapter counting{{.stale_after = 1000ms, .lost_after = 3000ms}};
    std::vector<std::string> interleaved;
    for (int step = 0; step < 300; ++step) {
        const auto sequence = static_cast<std::uint8_t>(step % 256);
        interleaved.push_back(
            line(frame_id(engine_node, efis::engine_can::engine_primary),
                 concat({{sequence}, u16le(2400), u16le(2370), u16le(720)})));
        interleaved.push_back(
            line(frame_id(engine_node, efis::engine_can::engine_fuel),
                 concat({{sequence}, u16le(1800), u16le(1750)})));
    }
    efis::io::RecordedCanSource many{interleaved};
    counting.consume(many, now);

    require(counting.statistics().sequence_wraps == 2,
            "both message counters must wrap exactly once each");
    const auto status = counting.health().status(engine_node);
    require(status.has_value(), "the engine node must be tracked");
    require(status->state == efis::NodeState::online,
            "a node that has just spoken must be online");
    require(status->regressions == 0,
            "a wrapping counter must not be read as a fault");

    // Il silenzio produce un evento anche senza frame in arrivo.
    counting.update(now + 5s);
    require(!counting.node_events().empty(),
            "prolonged silence must produce a node state change");
    require(counting.node_events().back().current == efis::NodeState::lost,
            "a node silent beyond the timeout must be declared lost");

    // --- Righe illeggibili nella cattura --------------------------------------
    efis::io::RecordedCanSource dirty{std::vector<std::string>{
        "# commento di candump",
        line(frame_id(engine_node, efis::engine_can::engine_fuel),
             concat({{0}, u16le(1800), u16le(1750)})),
        "riga troncata a meta",
    }};
    efis::EngineCanAdapter tolerant{};
    tolerant.consume(dirty, now);
    require(tolerant.statistics().frames_applied == 1,
            "the good frame in a dirty capture must still be read");
    require(dirty.malformed_lines() == 2,
            "unreadable lines must be counted, not silently skipped");

    // --- Cattura reale da vcan0, con traffico estraneo ------------------------
    //
    // Trentaquattro righe registrate sul nodo black box il 21 agosto 2026. Otto
    // sono i frame di Engine CAN V1 iniettati con cansend; le altre ventisei
    // appartengono a un altro protocollo che gia' viveva sul bus.
    //
    // E' la prova che conta, e ha cambiato il codice. Per come e' composto
    // l'identificatore, un frame CANopen 0x601 da otto byte ricade su nodo 12 e
    // messaggio ENGINE_PRIMARY: prima del controllo di lunghezza esatta l'adapter
    // lo accettava e ne ricavava un regime motore inventato. Non l'ha trovato il
    // ragionamento, l'ha trovato il bus.
    const std::filesystem::path recorded =
        std::filesystem::path{EFIS_TEST_DATA_DIR} / "can" /
        "engine-can-v1-vcan0-20260821.log";
    require(std::filesystem::exists(recorded),
            "the reference capture from vcan0 must be available");

    efis::io::RecordedCanSource from_bus{recorded};
    efis::EngineCanAdapter real{{.stale_after = 1000ms, .lost_after = 3000ms}};
    real.consume(from_bus, now);

    require(real.statistics().frames_applied == 8,
            "exactly the eight engine frames of the capture must be applied");
    require(real.statistics().frames_wrong_length > 0,
            "the foreign traffic must be refused on its length");
    require(real.health().status(12) == std::nullopt ||
                !real.health().status(12)->ever_seen(),
            "no measurement may be attributed to a node that never spoke our protocol");

    // I valori sono quelli iniettati con cansend, non quelli scritti a mano nelle
    // prove precedenti: hanno attraversato SocketCAN per davvero.
    const auto& bus = real.snapshot();
    require(bus.rpm.value == 2400.0, "the engine speed must survive the real bus");
    require(close_to(bus.manifold_pressure_inhg.value, 23.70),
            "the manifold pressure must survive the real bus");
    require(close_to(bus.fuel_left_gal.value, 18.0) &&
                close_to(bus.fuel_right_gal.value, 17.50),
            "the two tanks must survive the real bus");
    require(close_to(bus.cht_c[0].value, 178.0),
            "the cylinder head temperature must survive the real bus");
    require(close_to(bus.egt_c[3].value, 748.0),
            "the fourth exhaust temperature must survive the real bus");
    require(real.node_status().uptime_s == 3600,
            "the heartbeat must survive the real bus");

    // L'ultimo ENGINE_PRIMARY della cattura dichiara la sonda olio illeggibile.
    require(!bus.oil_pressure_psi.usable(),
            "the probe declared unreadable on the real bus must stay unavailable");
    require(bus.rpm.usable(),
            "the healthy quantities of that same frame must stay usable");

    std::cout << "All EFIS engine CAN tests passed\n";
}
