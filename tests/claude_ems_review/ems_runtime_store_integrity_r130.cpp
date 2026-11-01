// Audit indipendente EMS R1-30 — proprieta' reali di RuntimeStore.
//
// Appartiene alla review `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md`, dove il
// finding E-07 definisce lo store «fail-closed in modo corretto». Quella era
// una lettura del codice, non una misura: questo file la mette alla prova.
//
// `tests/engine_runtime_store_tests.cpp` copre tre casi — assenza del file,
// round trip e corruzione per append. Qui si coprono i casi che restano, e in
// particolare la domanda che il commento sul codice non pone: il checksum
// FNV-1a senza segreto rileva la corruzione accidentale, oppure anche
// l'alterazione deliberata? Le ore motore alimentano il programma di
// manutenzione (`ems_controller.cpp:594`), quindi la risposta e' una proprieta'
// del prodotto, non un dettaglio.
//
// Non e' dichiarato in CMake: l'incarico vieta di modificarlo. Si compila ed
// esegue con il comando riportato in `tests/claude_ems_review/README.md`.
// Scrive solo sotto una directory temporanea propria, che rimuove al termine.

#include "efis/core/engine_runtime_store.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, std::string_view name) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s  %.*s\n", condition ? "PASS" : "FAIL",
                static_cast<int>(name.size()), name.data());
}

// La stessa funzione di `src/core/engine_runtime_store.cpp`. Riprodurla qui e'
// il punto: chiunque possa scrivere il file puo' anche ricalcolare il valore,
// perche' non entra alcun segreto.
std::uint64_t fnv1a(std::string_view text) {
    std::uint64_t value = 14695981039346656037ULL;
    for (const unsigned char byte : text) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return value;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream bytes;
    bytes << input.rdbuf();
    return bytes.str();
}

// Riscrive il file con un corpo arbitrario e il checksum corretto per quel
// corpo, cioe' nella forma che lo store considera valida.
void write_with_valid_checksum(const std::filesystem::path& path,
                               const std::string& body) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << body << "checksum=" << std::hex << std::setw(16) << std::setfill('0')
        << fnv1a(body) << '\n';
}

std::string body_of(const std::filesystem::path& path) {
    const auto all = read_file(path);
    const auto marker = all.rfind("checksum=");
    return marker == std::string::npos ? all : all.substr(0, marker);
}

bool load_throws(const efis::engine::RuntimeStore& store) {
    try {
        static_cast<void>(store.load());
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

efis::engine::RuntimeRecord reference_record() {
    return {"asso-v-jolly-912-ul", "SERIAL-R130", {11.5, 7.25, 6.125, 0.0}, 42, 1700000000000ULL};
}

}  // namespace

int main() {
    std::printf("== RuntimeStore integrity, audit R1-30 ==\n");
    const auto root = std::filesystem::temp_directory_path() / "efis-claude-r130-store";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto path = root / "engine-runtime.state";

    // --- Round trip completo, non solo due campi -----------------------------
    {
        efis::engine::RuntimeStore store{path};
        check(!store.load(), "an absent store is a clean first start, not an error");
        const auto written = reference_record();
        store.save(written);
        const auto loaded = store.load();
        check(loaded.has_value(), "a saved record loads back");
        if (loaded) {
            check(loaded->profile_id == written.profile_id, "the profile id survives");
            check(loaded->engine_serial == written.engine_serial, "the engine serial survives");
            check(loaded->totals.master_hours == written.totals.master_hours,
                  "master hours survive exactly");
            check(loaded->totals.engine_hours == written.totals.engine_hours,
                  "engine hours survive exactly");
            check(loaded->totals.tach_hours == written.totals.tach_hours,
                  "tach hours survive exactly");
            check(loaded->totals.flight_hours == written.totals.flight_hours,
                  "flight hours survive exactly");
            check(loaded->revision == written.revision, "the revision survives");
            check(loaded->saved_wall_ms == written.saved_wall_ms, "the wall timestamp survives");
        }
    }

    // --- Forme non valide: lo store deve rifiutare ---------------------------
    {
        efis::engine::RuntimeStore store{path};
        const auto good_body = body_of(path);

        // Checksum assente del tutto.
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << good_body;
        }
        check(load_throws(store), "a record without a checksum is rejected");

        // Checksum di lunghezza sbagliata.
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << good_body << "checksum=dead\n";
        }
        check(load_throws(store), "a checksum of the wrong length is rejected");

        // Checksum della lunghezza giusta ma valore sbagliato.
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << good_body << "checksum=0000000000000000\n";
        }
        check(load_throws(store), "a checksum with the wrong value is rejected");

        // Schema non supportato, con checksum ricalcolato correttamente.
        {
            auto body = good_body;
            const auto position = body.find("schema=1");
            body.replace(position, 8, "schema=2");
            write_with_valid_checksum(path, body);
        }
        check(load_throws(store), "an unsupported schema is rejected even with a valid checksum");

        // Campo mancante, con checksum ricalcolato correttamente.
        {
            auto body = good_body;
            const auto start = body.find("tach_hours=");
            const auto end = body.find('\n', start);
            body.erase(start, end - start + 1);
            write_with_valid_checksum(path, body);
        }
        check(load_throws(store), "a missing field is rejected even with a valid checksum");

        // Valore non numerico, con checksum ricalcolato correttamente.
        {
            auto body = good_body;
            const auto start = body.find("engine_hours=");
            const auto end = body.find('\n', start);
            body.replace(start, end - start, "engine_hours=not-a-number");
            write_with_valid_checksum(path, body);
        }
        check(load_throws(store), "a non-numeric value is rejected even with a valid checksum");
    }

    // --- Rotazione del backup ------------------------------------------------
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        efis::engine::RuntimeStore store{path};
        auto first = reference_record();
        first.totals.engine_hours = 1.0;
        store.save(first);
        auto second = reference_record();
        second.totals.engine_hours = 2.0;
        store.save(second);
        const auto backup = std::filesystem::path{path.string() + ".bak"};
        check(std::filesystem::exists(backup), "the previous state is rotated to .bak");
        check(!std::filesystem::exists(std::filesystem::path{path.string() + ".new"}),
              "no temporary file is left behind");
        const auto loaded = store.load();
        check(loaded && loaded->totals.engine_hours == 2.0,
              "the active file holds the newest record");
        efis::engine::RuntimeStore backup_store{backup};
        const auto kept = backup_store.load();
        check(kept && kept->totals.engine_hours == 1.0,
              "the backup holds the previous record and is itself readable");
    }

    // --- La domanda che conta: corruzione o alterazione? ---------------------
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        efis::engine::RuntimeStore store{path};
        auto honest = reference_record();
        honest.totals.engine_hours = 7.25;
        store.save(honest);

        auto body = body_of(path);
        const auto start = body.find("engine_hours=");
        const auto end = body.find('\n', start);
        body.replace(start, end - start, "engine_hours=0.25");
        write_with_valid_checksum(path, body);

        const auto altered = store.load();
        check(altered.has_value(),
              "GAP: an altered record with a recomputed checksum loads cleanly");
        if (altered) {
            check(altered->totals.engine_hours == 0.25,
                  "GAP: the hour meter reads the altered value, not the saved one");
            check(altered->revision == honest.revision,
                  "GAP: the revision is unchanged, so nothing signals the edit");
        }
    }

    // --- Nessun controllo di plausibilita' sui totali ------------------------
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        efis::engine::RuntimeStore store{path};
        store.save(reference_record());
        auto body = body_of(path);
        const auto start = body.find("engine_hours=");
        const auto end = body.find('\n', start);
        body.replace(start, end - start, "engine_hours=-5");
        write_with_valid_checksum(path, body);
        const auto loaded = store.load();
        check(loaded.has_value(), "GAP: a negative hour total is accepted by the store");
        if (loaded) {
            check(loaded->totals.engine_hours == -5.0,
                  "GAP: the negative total reaches the caller unchanged");
        }
    }

    // --- Il formato non protegge i propri delimitatori ----------------------
    //
    // `payload()` scrive coppie chiave=valore una per riga, senza alcun escape,
    // e `load()` ricostruisce la mappa con `emplace`, che **conserva la prima
    // occorrenza**. Un ritorno a capo dentro un campo di testo non viene quindi
    // soltanto troncato: le righe che segue vengono interpretate come campi, e
    // poiche' matricola e profilo sono scritti prima dei totali, un campo
    // iniettato precede quello reale e vince.
    //
    // Il checksum non puo' accorgersene: viene calcolato sul corpo che contiene
    // gia' l'iniezione, quindi il file e' perfettamente coerente con se stesso.
    // E' un difetto del serializzatore, non del controllo d'integrita'.
    {
        const auto round_trip = [&](const std::string& serial) {
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root);
            efis::engine::RuntimeStore store{path};
            efis::engine::RuntimeRecord written{
                "asso-v-jolly-912-ul", serial, {11.5, 7.25, 6.125, 0.0}, 42, 1700000000000ULL};
            store.save(written);
            return store.load();
        };

        const auto truncated = round_trip("AB\nCD");
        check(truncated && truncated->engine_serial == "AB",
              "GAP: a newline in a text field truncates it silently instead of being rejected");

        const auto hours = round_trip("X\nengine_hours=999");
        check(hours && hours->totals.engine_hours == 999.0,
              "GAP: a field injected through the serial overrides the saved engine hours");

        const auto revision = round_trip("X\nrevision=1");
        check(revision && revision->revision == 1U,
              "GAP: the same path overrides the saved revision");

        // Il guardiano dello schema regge su una riga sostituita e non su una
        // iniettata, perche' `schema=1` e' scritto per primo e `emplace` lo
        // conserva: il record si carica benche' dichiari anche schema 2.
        const auto schema = round_trip("X\nschema=2");
        check(schema.has_value(),
              "GAP: an injected schema line does not reach the unsupported-schema guard");
    }

    std::filesystem::remove_all(root);
    std::printf("== %d checks, %d failures ==\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
