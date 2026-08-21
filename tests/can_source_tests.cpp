// Prove del trasporto CAN, secondo passo di P4.
//
// La sorgente SocketCAN vera si prova sul nodo black box con vcan0, dove i frame
// li inietta cansend: e' la sola prova che dimostri il trasporto, e sta in
// tools/can/bench_vcan.sh. Qui si verifica cio' che si puo' verificare ovunque,
// cioe' la lettura delle registrazioni e il comportamento dichiarato della
// sorgente quando l'interfaccia non c'e'.

#include "efis/io/can_source.hpp"
#include "efis/io/socketcan_source.hpp"
#include "efis/providers/esp32_can_adapter.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};

    // --- Formato candump ----------------------------------------------------
    //
    // E' il formato degli strumenti standard. Sceglierlo significa che una
    // registrazione fatta sul banco si rilegge qui senza conversioni.
    {
        std::istringstream log{
            "(1755739800.100000) vcan0 100#7CFF9C0402070FFF\n"
            "(1755739800.200000) vcan0 101#6A03800CD4000A00\n"
            "(1755739800.300000) vcan0 302#010B0C00000000\n"};

        const auto parsed = efis::io::parse_candump(log);
        require(parsed.frames.size() == 3, "every well formed line must yield a frame");
        require(parsed.malformed_lines == 0, "well formed lines must not be counted as bad");
        require(parsed.frames[0].id == 0x100, "the identifier must decode as hexadecimal");
        require(parsed.frames[0].length == 8, "the payload length comes from the digits");
        require(parsed.frames[0].data[0] == 0x7C, "payload bytes must decode in order");
        require(parsed.frames[2].length == 7,
                "a payload shorter than eight bytes must keep its real length");
    }

    // --- Righe illeggibili, errori e richieste remote ------------------------
    //
    // Un frame di errore descrive lo stato del bus, non una grandezza fisica.
    // Consegnarlo all'adapter significherebbe leggere un cavo interrotto come
    // se fosse un assetto.
    {
        std::istringstream log{
            "(1755739800.100000) vcan0 100#7CFF9C0402070FFF\n"
            "questa riga non e' un frame\n"
            "(1755739800.150000) vcan0 20000004#0000000000000000\n"
            "(1755739800.160000) vcan0 123#R\n"
            "(1755739800.170000) vcan0 100#7CF\n"};

        const auto parsed = efis::io::parse_candump(log);
        require(parsed.frames.size() == 1, "only the valid frame must be delivered");
        require(parsed.malformed_lines == 2,
                "the unreadable line and the odd length payload must both be counted");
        require(parsed.error_frames == 1, "an error frame must be counted, not delivered");
        require(parsed.remote_frames == 1, "a remote request carries no measurement");
    }

    // --- Una registrazione si rilegge e si riavvolge -------------------------
    {
        std::istringstream log{
            "(1755739800.100000) vcan0 100#7CFF9C0402070FFF\n"
            "(1755739800.200000) vcan0 101#6A03800CD4000A00\n"};
        auto parsed = efis::io::parse_candump(log);

        efis::io::RecordedCanSource source{parsed.frames};
        require(!source.exhausted(), "a fresh recording is not exhausted");

        std::vector<efis::can::Frame> buffer(4);
        const std::size_t read = source.read(buffer);
        require(read == 2, "the whole recording must be readable in one go");
        require(source.exhausted(), "after the last frame the recording is exhausted");

        source.rewind();
        require(!source.exhausted(),
                "rewinding must make the same recording replayable: two runs must agree");
        require(source.read(buffer) == 2, "the rewound recording must yield the same frames");
    }

    // --- Dalla registrazione all'adapter, catena intera ----------------------
    //
    // I byte di questi frame sono quelli del protocollo del nodo sensori: se le
    // scale o l'ordine fossero sbagliati, i valori uscirebbero diversi.
    {
        // 0x100: pitch -132 (=-1,32°), roll 412 (=4,12°), heading 9000 (=90,00°),
        // sensori tutti buoni, calibrazione piena.
        // 0x101: IAS 874 (=87,4 kt), quota 3200 ft, pressione 132 (=1013,2 hPa),
        // differenziale 475 (=47,5 Pa).
        std::istringstream log{
            "(1755739800.100000) vcan0 100#7CFF9C01282307FF\n"
            "(1755739800.200000) vcan0 101#6A03800C8400DB01\n"
            "(1755739800.300000) vcan0 302#010B0C0000000000\n"};

        auto parsed = efis::io::parse_candump(log);
        require(parsed.frames.size() == 3, "the three frames must parse");

        efis::io::RecordedCanSource source{parsed.frames};
        efis::Esp32CanAdapter adapter;

        std::vector<efis::can::Frame> buffer(8);
        const std::size_t read = source.read(buffer);
        adapter.consume(std::span<const efis::can::Frame>{buffer.data(), read},
                        epoch + 1s);

        require(adapter.statistics().frames_applied == 3,
                "every frame of the recording must reach the adapter");
        require(std::abs(adapter.snapshot().attitude.roll_deg.value - 4.12) < 0.02,
                "roll must survive the whole chain, from the text log to the sample");
        require(std::abs(adapter.snapshot().air_data.indicated_airspeed_kt.value - 87.4) < 0.05,
                "indicated airspeed must survive the whole chain");
        require(adapter.snapshot().attitude.roll_deg.usable(),
                "with sensors declared good and full calibration the attitude is usable");
        require(adapter.sensor_state().gnss_satellites == 11,
                "the satellite count must arrive from the status frame");
    }

    // --- Una sorgente che non si apre lo dichiara ---------------------------
    //
    // Un bus scollegato e un bus fermo producono entrambi zero frame. Senza
    // questa distinzione l'assenza di un'interfaccia verrebbe mostrata come un
    // nodo che tace, che è una diagnosi diversa e sbagliata.
    {
        efis::io::SocketCanSource source{"efis-interfaccia-inesistente"};
        require(!source.is_open(), "a source that has not been opened is not open");
        require(!source.open(), "opening a non existent interface must fail");
        require(!source.error().empty(),
                "a source that cannot open must say why, not stay silent");
        require(source.exhausted(),
                "a closed source will produce nothing, and must say so");

        std::vector<efis::can::Frame> buffer(4);
        require(source.read(buffer) == 0, "a closed source reads nothing");
    }

    std::cout << "All EFIS CAN transport tests passed\n";
}
