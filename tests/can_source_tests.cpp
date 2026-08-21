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
#include <fstream>
#include <filesystem>
#include <algorithm>
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

    // --- Una registrazione vera, presa da vcan0 -------------------------------
    //
    // Le prove qui sopra usano righe scritte nel test: dimostrano che il parser
    // fa quello che ci aspettiamo. Questa usa una registrazione acquisita da un
    // vcan0 vero il 21 agosto 2026, e dimostra una cosa diversa: che il parser
    // regge cio' che candump produce davvero, con i suoi tempi, la sua
    // spaziatura e le sue lunghezze variabili.
    //
    // Gli identificativi appartengono a ENGINE_CAN_V1, protocollo poi ritirato
    // in favore di ESP32_CAN_V1. La registrazione resta valida lo stesso, perche'
    // qui non si interpreta il significato dei frame ma il formato del testo: e'
    // la ragione per cui e' stata tenuta invece di essere buttata con il resto
    // di quello stack.
    {
        const std::filesystem::path recorded =
            std::filesystem::path{EFIS_TEST_DATA_DIR} / "can" /
            "engine-can-v1-vcan0-20260821.log";
        require(std::filesystem::exists(recorded),
                "the recovered vcan0 recording must be available to the tests");

        std::ifstream file{recorded};
        const auto parsed = efis::io::parse_candump(file);

        require(parsed.frames.size() == 33,
                "every line of the real recording must yield a frame");
        require(parsed.malformed_lines == 0,
                "a recording taken from candump must contain no unreadable line");
        require(parsed.error_frames == 0 && parsed.remote_frames == 0,
                "this recording carries only data frames");

        // Lunghezze diverse nella stessa registrazione: e' il caso che una riga
        // scritta a mano tende a non coprire, perche' si scrive sempre otto.
        std::size_t shortest = 8;
        std::size_t longest = 0;
        for (const auto& frame : parsed.frames) {
            shortest = std::min<std::size_t>(shortest, frame.length);
            longest = std::max<std::size_t>(longest, frame.length);
        }
        require(shortest == 5 && longest == 8,
                "the real recording must exercise payloads from five to eight bytes");

        // Identificativi a tre cifre, letti in esadecimale: 0x601 e non 601.
        std::size_t on_601 = 0;
        for (const auto& frame : parsed.frames) {
            if (frame.id == 0x601U) ++on_601;
        }
        require(on_601 == 20,
                "the identifier must be read as hexadecimal, on real data too");

        // La stessa registrazione riletta due volte deve dare gli stessi frame:
        // senza questo il replay non e' un banco.
        efis::io::RecordedCanSource source{parsed.frames};
        std::vector<efis::can::Frame> first(64);
        const auto read_first = source.read(first);
        source.rewind();
        std::vector<efis::can::Frame> second(64);
        const auto read_second = source.read(second);
        require(read_first == read_second && read_first == parsed.frames.size(),
                "replaying the recording must deliver the same number of frames");
        require(first[0].id == second[0].id && first[0].length == second[0].length,
                "and the same frames, in the same order");
    }

    std::cout << "All EFIS CAN transport tests passed\n";
}
