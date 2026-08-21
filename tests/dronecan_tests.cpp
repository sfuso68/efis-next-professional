// Prove del decodificatore DroneCAN.
//
// La prova che conta e' la registrazione reale: un ice.reciprocating.Status
// costruito dall'implementazione di riferimento, iniettato su vcan0 del nodo
// black box e ripreso con candump. Se il decodificatore scritto qui ne ricava
// gli stessi valori, lo fa perche' rispetta il protocollo e non perche' condivide
// un malinteso con chi ha scritto le prove.

#include "efis/io/can_source.hpp"
#include "efis/providers/dronecan.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close_to(double value, double expected, double tolerance = 0.05) {
    return std::abs(value - expected) < tolerance;
}

constexpr double kelvin = 273.15;

}  // namespace

int main() {
    // --- Scomposizione dell'identificatore e della coda -----------------------
    const auto header = efis::dronecan::decode_message_id(0x1404601FU);
    require(header.priority == 20, "the priority is the top five bits");
    require(header.data_type_id == 1120, "the data type is the middle sixteen bits");
    require(!header.service, "a broadcast message is not a service frame");
    require(header.source_node == 31, "the source node is the low seven bits");

    const auto first = efis::dronecan::decode_tail(0x85);
    require(first.start && !first.end && !first.toggle && first.transfer_id == 5,
            "the tail byte of a first frame must decode");
    const auto last = efis::dronecan::decode_tail(0x45);
    require(!last.start && last.end && !last.toggle && last.transfer_id == 5,
            "the tail byte of a last frame must decode");

    // La signature entra nel CRC: un valore sbagliato non da' errore, fa
    // scartare in silenzio tutti i trasferimenti di quel tipo.
    const auto* status = efis::dronecan::find_type(1120);
    require(status != nullptr, "reciprocating.Status must be in the table");
    require(status->signature == 0xD38AA3EE75537EC6ULL,
            "the type signature must match the protocol definition");
    require(efis::dronecan::find_type(341) != nullptr,
            "NodeStatus must be in the table");
    require(efis::dronecan::find_type(999999) == nullptr,
            "an unknown data type must not resolve");

    // --- La registrazione reale ----------------------------------------------
    const std::filesystem::path capture =
        std::filesystem::path{EFIS_TEST_DATA_DIR} / "can" /
        "dronecan-ice-status-vcan0-20260821.log";
    require(std::filesystem::exists(capture),
            "the reference capture from vcan0 must be available");

    std::ifstream file{capture};
    const auto log = efis::io::parse_candump(file);
    require(!log.frames.empty(), "the capture must contain frames");

    efis::dronecan::Reassembler reassembler;
    std::optional<efis::dronecan::DecodedMessage> engine;
    for (const auto& frame : log.frames) {
        if (auto decoded = reassembler.push(frame)) {
            if (decoded->type->data_type_id == 1120) engine = decoded;
        }
    }

    require(reassembler.statistics().transfers == 1,
            "the capture must yield exactly one complete transfer");
    require(reassembler.statistics().crc_errors == 0,
            "the transfer CRC must verify: a mismatch would mean a wrong signature");
    require(reassembler.statistics().toggle_errors == 0,
            "no frame of the capture may be out of sequence");
    require(reassembler.statistics().unknown_types > 0,
            "the foreign traffic on the shared bus must be counted, not decoded");

    require(engine.has_value(), "the engine status must be reassembled");
    require(engine->source_node == 31, "the source node must survive reassembly");
    require(engine->transfer_id == 5, "the transfer id must survive reassembly");

    // I valori sono quelli trasmessi. Le unita' sono quelle del DSDL, kelvin e
    // kilopascal, non quelle del cruscotto: e' la ragione per cui il campione
    // conserva il grezzo accanto al convertito.
    require(engine->value("engine_speed_rpm") == 2400.0,
            "the engine speed must decode from a seventeen bit field");
    require(close_to(*engine->value("intake_manifold_pressure_kpa"), 80.25, 0.02),
            "the manifold pressure must decode from a half precision float");
    require(close_to(*engine->value("oil_pressure"), 496.4, 0.2),
            "the oil pressure must decode");
    require(close_to(*engine->value("oil_temperature") - kelvin, 94.0, 0.2),
            "the oil temperature must decode, and it is in kelvin on the wire");
    require(close_to(*engine->value("fuel_pressure"), 35.85, 0.05),
            "the fuel pressure must decode");
    require(engine->value("throttle_position_percent") == 65.0,
            "a seven bit field packed among floats must keep its position");
    require(engine->value("state") == 2.0, "the two bit state field must decode");

    // I quattro cilindri. L'array chiude il messaggio e i suoi elementi hanno
    // almeno otto bit, quindi il DSDL ne omette il numero: si ricava dai bit che
    // restano. Cercare un prefisso che non c'e' sposterebbe ogni valore.
    require(engine->count("cylinder_head_temperature") == 4,
            "the tail array must yield four cylinders without a length prefix");
    require(close_to(*engine->value("cylinder_head_temperature", 0) - kelvin, 178.0, 0.2),
            "the first cylinder head temperature must decode");
    require(close_to(*engine->value("cylinder_head_temperature", 3) - kelvin, 187.0, 0.2),
            "the fourth cylinder head temperature must decode");
    require(close_to(*engine->value("exhaust_gas_temperature", 0) - kelvin, 742.0, 0.5),
            "the first exhaust gas temperature must decode");
    require(close_to(*engine->value("exhaust_gas_temperature", 3) - kelvin, 760.0, 0.5),
            "the fourth exhaust gas temperature must decode");

    require(!engine->value("cylinder_head_temperature", 4).has_value(),
            "a cylinder that was never transmitted must not exist");
    require(!engine->value("inesistente").has_value(),
            "an unknown field must be absent, never zero");

    // --- Un frame alterato non deve passare -----------------------------------
    //
    // Il CRC del trasferimento e' l'unica difesa contro un messaggio ricucito
    // male: senza questo controllo un byte cambiato produrrebbe un valore
    // leggermente sbagliato, che e' peggio di un valore mancante.
    efis::dronecan::Reassembler corrupted;
    bool delivered = false;
    for (std::size_t index = 0; index < log.frames.size(); ++index) {
        auto frame = log.frames[index];
        if (frame.id == 0x1404601FU && index > 0 && frame.length > 2) {
            frame.data[1] = static_cast<std::uint8_t>(frame.data[1] ^ 0xFF);
        }
        if (corrupted.push(frame).has_value()) delivered = true;
    }
    require(!delivered, "a corrupted transfer must never be delivered");
    require(corrupted.statistics().crc_errors == 1,
            "the corruption must be caught by the transfer CRC");

    // --- Un frame mancante non deve produrre un messaggio ricucito ------------
    efis::dronecan::Reassembler truncated;
    bool completed = false;
    for (std::size_t index = 0; index < log.frames.size(); ++index) {
        if (log.frames[index].id == 0x1404601FU && index % 3 == 0 && index > 0) continue;
        if (truncated.push(log.frames[index]).has_value()) completed = true;
    }
    require(!completed || truncated.statistics().toggle_errors > 0,
            "a transfer missing a frame must be abandoned, not stitched together");

    std::cout << "All EFIS DroneCAN tests passed\n";
}
