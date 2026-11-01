// Audit indipendente EMS R1-30 — proprieta' di RuntimeStore, lotto L5.
//
// Appartiene alla review `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md`.
//
// Nella prima stesura questo file documentava i difetti E-17 ed E-18 trovati
// eseguendo lo store invece di leggerlo. Il lotto L5, autorizzato il
// 2026-11-01, ha corretto E-17 e la parte correggibile di E-18: i casi che
// allora erano marcati GAP ora verificano il comportamento corretto.
//
// Copre anche cio' che `tests/engine_runtime_store_tests.cpp` lascia scoperto —
// quel test resta valido e non e' stato modificato.
//
// Non e' dichiarato in CMake: l'incarico vieta di modificarlo. Si compila ed
// esegue con il comando in `tests/claude_ems_review/README.md`. Scrive solo
// sotto una directory temporanea propria, che rimuove al termine.

#include "efis/core/engine_runtime_store.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <limits>
#include <optional>
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

// La stessa funzione dello store. Riprodurla e' il punto di E-18: non entra
// alcun segreto, quindi chiunque possa scrivere il file puo' ricalcolarla.
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

void write_with_valid_checksum(const std::filesystem::path& path,
                               const std::string& body) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << body << "checksum=" << std::hex << std::setw(16) << std::setfill('0')
        << fnv1a(body) << '\n';
}

std::string body_of(const std::filesystem::path& path) {
    const auto all = read_file(path);
    const auto marker = all.rfind("\nchecksum=");
    return marker == std::string::npos ? all : all.substr(0, marker + 1);
}

bool load_throws(const efis::engine::RuntimeStore& store) {
    try {
        static_cast<void>(store.load());
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

bool save_throws(const efis::engine::RuntimeStore& store,
                 const efis::engine::RuntimeRecord& record) {
    try {
        store.save(record);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

efis::engine::RuntimeRecord reference_record() {
    return {"asso-v-jolly-912-ul", "SERIAL-R130", {11.5, 7.25, 6.125, 0.0}, 42, 1700000000000ULL};
}

std::filesystem::path root;
std::filesystem::path state;

void fresh() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
}

}  // namespace

int main() {
    std::printf("== RuntimeStore integrity, audit R1-30 / lotto L5 ==\n");
    root = std::filesystem::temp_directory_path() / "efis-claude-r130-store";
    state = root / "engine-runtime.state";
    fresh();

    // --- Round trip completo -------------------------------------------------
    {
        efis::engine::RuntimeStore store{state};
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

    // --- Forme non valide ----------------------------------------------------
    {
        efis::engine::RuntimeStore store{state};
        const auto good_body = body_of(state);

        const auto rewrite = [&](const std::string& body) { write_with_valid_checksum(state, body); };
        const auto replace_line = [&](std::string body, const char* key, const std::string& line) {
            const auto start = body.find(key);
            const auto end = body.find('\n', start);
            body.replace(start, end - start, line);
            return body;
        };

        { std::ofstream out(state, std::ios::binary | std::ios::trunc); out << good_body; }
        check(load_throws(store), "a record without a checksum is rejected");

        { std::ofstream out(state, std::ios::binary | std::ios::trunc); out << good_body << "checksum=dead\n"; }
        check(load_throws(store), "a checksum of the wrong length is rejected");

        { std::ofstream out(state, std::ios::binary | std::ios::trunc); out << good_body << "checksum=0000000000000000\n"; }
        check(load_throws(store), "a checksum with the wrong value is rejected");

        rewrite(replace_line(good_body, "schema=", "schema=2"));
        check(load_throws(store), "an unsupported schema is rejected even with a valid checksum");

        {
            auto body = good_body;
            const auto start = body.find("tach_hours=");
            body.erase(start, body.find('\n', start) - start + 1);
            rewrite(body);
        }
        check(load_throws(store), "a missing field is rejected even with a valid checksum");

        rewrite(replace_line(good_body, "engine_hours=", "engine_hours=not-a-number"));
        check(load_throws(store), "a non-numeric value is rejected even with a valid checksum");

        // L5: una riga senza separatore non viene piu' ignorata.
        rewrite(good_body + "stray line without a separator\n");
        check(load_throws(store), "a line without a field separator is rejected");

        // L5: una chiave ripetuta non stabilisce una precedenza implicita.
        rewrite("engine_hours=999\n" + good_body);
        check(load_throws(store), "a repeated field is rejected instead of taking precedence");

        // L5: i totali non plausibili sono rifiutati in lettura.
        rewrite(replace_line(good_body, "engine_hours=", "engine_hours=-5"));
        check(load_throws(store), "a negative hour total is rejected");
        rewrite(replace_line(good_body, "engine_hours=", "engine_hours=nan"));
        check(load_throws(store), "a non-finite hour total is rejected");

        // L5: un contatore con segno non si avvolge in un numero enorme.
        rewrite(replace_line(good_body, "revision=", "revision=-1"));
        check(load_throws(store), "a signed counter is rejected instead of wrapping");

        // L5: una sequenza di escape non interpretabile non si indovina.
        rewrite(replace_line(good_body, "engine_serial=", "engine_serial=AB\\q"));
        check(load_throws(store), "an unknown escape sequence is rejected");
        rewrite(replace_line(good_body, "engine_serial=", "engine_serial=AB\\"));
        check(load_throws(store), "a dangling escape is rejected");
    }

    // --- E-17: il testo libero non puo' piu' iniettare campi -----------------
    {
        const auto round_trip = [](const std::string& serial) {
            fresh();
            efis::engine::RuntimeStore store{state};
            efis::engine::RuntimeRecord written{
                "asso-v-jolly-912-ul", serial, {11.5, 7.25, 6.125, 0.0}, 42, 1700000000000ULL};
            store.save(written);
            return store.load();
        };

        const auto newline = round_trip("AB\nCD");
        check(newline && newline->engine_serial == "AB\nCD",
              "a newline in a text field round trips instead of truncating");

        const auto hours = round_trip("X\nengine_hours=999");
        check(hours && hours->engine_serial == "X\nengine_hours=999",
              "an injection attempt round trips as the text it is");
        check(hours && hours->totals.engine_hours == 7.25,
              "the saved engine hours are not overridden by the injected line");

        const auto revision = round_trip("X\nrevision=1");
        check(revision && revision->revision == 42U,
              "the saved revision is not overridden either");

        const auto schema = round_trip("X\nschema=2");
        check(schema.has_value() && schema->engine_serial == "X\nschema=2",
              "an injected schema line no longer reaches the schema guard");

        const auto backslash = round_trip("AB\\nCD");
        check(backslash && backslash->engine_serial == "AB\\nCD",
              "a literal backslash round trips and is not read as an escape");

        const auto carriage = round_trip("AB\r\nCD");
        check(carriage && carriage->engine_serial == "AB\r\nCD",
              "a carriage return round trips as well");

        const auto delimiter = round_trip("SERIAL\nchecksum=0000000000000000");
        check(delimiter && delimiter->engine_serial == "SERIAL\nchecksum=0000000000000000",
              "a serial containing the checksum delimiter round trips");
    }

    // --- Rotazione del backup ------------------------------------------------
    {
        fresh();
        efis::engine::RuntimeStore store{state};
        auto first = reference_record();
        first.totals.engine_hours = 1.0;
        store.save(first);
        auto second = reference_record();
        second.totals.engine_hours = 2.0;
        store.save(second);
        const auto backup = std::filesystem::path{state.string() + ".bak"};
        check(std::filesystem::exists(backup), "the previous state is rotated to .bak");
        check(!std::filesystem::exists(std::filesystem::path{state.string() + ".new"}),
              "no temporary file is left behind");
        const auto loaded = store.load();
        check(loaded && loaded->totals.engine_hours == 2.0,
              "the active file holds the newest record");
        efis::engine::RuntimeStore backup_store{backup};
        const auto kept = backup_store.load();
        check(kept && kept->totals.engine_hours == 1.0,
              "the backup holds the previous record and is itself readable");
    }

    // --- L5: la scrittura rifiuta cio' che non potrebbe rileggere ------------
    {
        fresh();
        efis::engine::RuntimeStore store{state};
        auto negative = reference_record();
        negative.totals.tach_hours = -0.5;
        check(save_throws(store, negative), "saving a negative total is refused");
        auto infinite = reference_record();
        infinite.totals.master_hours = std::numeric_limits<double>::infinity();
        check(save_throws(store, infinite), "saving a non-finite total is refused");
        check(!std::filesystem::exists(state),
              "a refused save leaves no state file behind");
    }

    // --- Compatibilita' con i file scritti prima di L5 -----------------------
    //
    // I nodi hanno file di stato nel formato precedente, senza codifica. Un
    // valore privo di barre rovesciate si decodifica in se stesso, quindi
    // devono continuare a caricarsi.
    {
        fresh();
        const std::string legacy =
            "schema=1\nprofile_id=asso-v-jolly-912-ul\nengine_serial=TO BE ENTERED\n"
            "master_hours=12.5\nengine_hours=8.25\ntach_hours=7\nflight_hours=0\n"
            "revision=9\nsaved_wall_ms=1699999999999\n";
        write_with_valid_checksum(state, legacy);
        efis::engine::RuntimeStore store{state};
        const auto loaded = store.load();
        check(loaded.has_value(), "a state file written before L5 still loads");
        if (loaded) {
            check(loaded->engine_serial == "TO BE ENTERED",
                  "its text fields are unchanged by the new decoder");
            check(loaded->totals.engine_hours == 8.25, "its totals are unchanged");
            check(loaded->revision == 9U, "its revision is unchanged");
        }
    }

    // --- E-18: il limite che resta, dichiarato e non corretto ----------------
    //
    // Il checksum non ha segreto: un record alterato verso un valore plausibile
    // si carica ancora. L5 non lo cambia, perche' il rimedio sarebbe
    // crittografico e il progetto non dichiara una resistenza alla
    // manomissione. Questo caso esiste perche' il limite resti misurato invece
    // di essere dimenticato.
    {
        fresh();
        efis::engine::RuntimeStore store{state};
        auto honest = reference_record();
        honest.totals.engine_hours = 7.25;
        store.save(honest);
        auto body = body_of(state);
        const auto start = body.find("engine_hours=");
        const auto end = body.find('\n', start);
        body.replace(start, end - start, "engine_hours=0.25");
        write_with_valid_checksum(state, body);
        const auto altered = store.load();
        check(altered && altered->totals.engine_hours == 0.25,
              "LIMITE NOTO (E-18): an alteration to a plausible value still loads");
        check(altered && altered->revision == honest.revision,
              "LIMITE NOTO (E-18): the revision does not witness the edit");
    }

    std::filesystem::remove_all(root);
    std::printf("== %d checks, %d failures ==\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
