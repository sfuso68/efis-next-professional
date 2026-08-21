#include "efis/instruments/panel_store.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace efis::instruments {
namespace {

constexpr int supported_schema = 1;

// --- Scrittura ---------------------------------------------------------------

std::string quote(const std::string& value) {
    std::string out = "\"";
    for (const char character : value) {
        // Lo schema non prevede testo con virgolette o barre rovesce, ma un
        // nome di pagina arriva dal pilota: sfuggirle e' l'unico modo perche' un
        // apostrofo o una barra non producano un file che non si rilegge.
        if (character == '"' || character == '\\') out.push_back('\\');
        out.push_back(character);
    }
    out.push_back('"');
    return out;
}

// --- Lettura -----------------------------------------------------------------
//
// Lettore scritto per questo schema soltanto, e deliberatamente severo. Un
// lettore permissivo accetterebbe un file quasi giusto e produrrebbe una
// disposizione quasi giusta, che e' la condizione peggiore: il pannello si apre
// e manca qualcosa, senza che nulla lo dichiari.
class Reader final {
public:
    explicit Reader(const std::string& text) : text_(text) {}

    void skip() {
        while (position_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[position_])) != 0) {
            ++position_;
        }
    }

    bool consume(char expected) {
        skip();
        if (position_ >= text_.size() || text_[position_] != expected) return false;
        ++position_;
        return true;
    }

    bool peek(char expected) {
        skip();
        return position_ < text_.size() && text_[position_] == expected;
    }

    bool string(std::string& out) {
        skip();
        if (position_ >= text_.size() || text_[position_] != '"') return false;
        ++position_;
        out.clear();
        while (position_ < text_.size() && text_[position_] != '"') {
            if (text_[position_] == '\\' && position_ + 1 < text_.size()) ++position_;
            out.push_back(text_[position_++]);
        }
        if (position_ >= text_.size()) return false;
        ++position_;
        return true;
    }

    bool number(double& out) {
        skip();
        const std::size_t start = position_;
        if (position_ < text_.size() && (text_[position_] == '-' || text_[position_] == '+')) {
            ++position_;
        }
        while (position_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[position_])) != 0 ||
                text_[position_] == '.' || text_[position_] == 'e' ||
                text_[position_] == 'E' || text_[position_] == '-' ||
                text_[position_] == '+')) {
            ++position_;
        }
        if (position_ == start) return false;
        try {
            out = std::stod(text_.substr(start, position_ - start));
        } catch (...) {
            return false;
        }
        return true;
    }

    // Salta un valore qualunque. Non serve ad accettare l'ignoto: serve a
    // saltare cio' che si e' gia' deciso di rifiutare, senza perdere la
    // posizione.
    bool skip_value() {
        skip();
        if (position_ >= text_.size()) return false;
        const char first = text_[position_];
        if (first == '"') {
            std::string ignored;
            return string(ignored);
        }
        if (first == '{' || first == '[') {
            const char open = first;
            const char close = open == '{' ? '}' : ']';
            int depth = 0;
            while (position_ < text_.size()) {
                const char character = text_[position_];
                if (character == '"') {
                    std::string ignored;
                    if (!string(ignored)) return false;
                    continue;
                }
                if (character == open) ++depth;
                if (character == close) {
                    --depth;
                    ++position_;
                    if (depth == 0) return true;
                    continue;
                }
                ++position_;
            }
            return false;
        }
        double ignored = 0.0;
        if (number(ignored)) return true;
        // true, false, null: non previsti da questo schema.
        return false;
    }

    [[nodiscard]] bool exhausted() {
        skip();
        return position_ >= text_.size();
    }

private:
    const std::string& text_;
    std::size_t position_{};
};

ParseResult fail(ParseProblem problem, std::string field = {}) {
    ParseResult result;
    result.problem = problem;
    result.field = std::move(field);
    return result;
}

bool read_slot(Reader& reader, Slot& slot) {
    if (!reader.consume('{')) return false;
    bool have_row = false;
    bool have_column = false;
    bool have_kind = false;
    while (!reader.peek('}')) {
        std::string key;
        if (!reader.string(key) || !reader.consume(':')) return false;
        double value = 0.0;
        if (key == "row") {
            if (!reader.number(value)) return false;
            slot.row = static_cast<int>(value);
            have_row = true;
        } else if (key == "column") {
            if (!reader.number(value)) return false;
            slot.column = static_cast<int>(value);
            have_column = true;
        } else if (key == "instrument") {
            if (!reader.number(value)) return false;
            slot.kind = static_cast<InstrumentKind>(static_cast<std::uint16_t>(value));
            have_kind = true;
        } else {
            return false;  // chiave sconosciuta: si rifiuta, non si indovina
        }
        if (!reader.consume(',')) break;
    }
    if (!reader.consume('}')) return false;
    return have_row && have_column && have_kind;
}

bool read_page(Reader& reader, PanelPage& page) {
    if (!reader.consume('{')) return false;
    while (!reader.peek('}')) {
        std::string key;
        if (!reader.string(key) || !reader.consume(':')) return false;
        if (key == "name") {
            if (!reader.string(page.name)) return false;
        } else if (key == "rows" || key == "columns") {
            double value = 0.0;
            if (!reader.number(value)) return false;
            (key == "rows" ? page.rows : page.columns) = static_cast<int>(value);
        } else if (key == "slots") {
            if (!reader.consume('[')) return false;
            while (!reader.peek(']')) {
                Slot slot;
                if (!read_slot(reader, slot)) return false;
                page.slots.push_back(slot);
                if (!reader.consume(',')) break;
            }
            if (!reader.consume(']')) return false;
        } else {
            return false;
        }
        if (!reader.consume(',')) break;
    }
    return reader.consume('}');
}

std::string timestamp_suffix() {
    // Il tempo di sistema non e' disponibile nel core senza dipendenze, e la
    // black box usa il tempo monotono. Per distinguere le copie basta un
    // contatore crescente all'interno della sessione: il registro di controllo
    // porta gia' l'ordine degli eventi.
    static int counter = 0;
    ++counter;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d", counter);
    return buffer;
}

}  // namespace

std::string to_json(const PanelLayout& layout) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema_version\": " << layout.schema_version << ",\n";
    out << "  \"revision\": " << layout.revision << ",\n";
    out << "  \"profile_id\": " << quote(layout.profile_id) << ",\n";
    out << "  \"capability\": "
        << (layout.capability == Capability::professional ? "\"professional\""
                                                          : "\"standard\"")
        << ",\n";
    out << "  \"pages\": [\n";
    for (std::size_t page_index = 0; page_index < layout.pages.size(); ++page_index) {
        const PanelPage& page = layout.pages[page_index];
        out << "    {\n";
        out << "      \"name\": " << quote(page.name) << ",\n";
        out << "      \"rows\": " << page.rows << ",\n";
        out << "      \"columns\": " << page.columns << ",\n";
        out << "      \"slots\": [\n";
        for (std::size_t slot_index = 0; slot_index < page.slots.size(); ++slot_index) {
            const Slot& slot = page.slots[slot_index];
            out << "        {\"row\": " << slot.row << ", \"column\": " << slot.column
                << ", \"instrument\": " << static_cast<int>(slot.kind) << "}";
            if (slot_index + 1 < page.slots.size()) out << ",";
            out << "\n";
        }
        out << "      ]\n";
        out << "    }";
        if (page_index + 1 < layout.pages.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

ParseResult from_json(const std::string& text) {
    Reader reader{text};
    PanelLayout layout;
    bool have_schema = false;

    if (!reader.consume('{')) return fail(ParseProblem::malformed);

    while (!reader.peek('}')) {
        std::string key;
        if (!reader.string(key)) return fail(ParseProblem::malformed);
        if (!reader.consume(':')) return fail(ParseProblem::malformed, key);

        if (key == "schema_version") {
            double value = 0.0;
            if (!reader.number(value)) return fail(ParseProblem::wrong_type, key);
            layout.schema_version = static_cast<int>(value);
            have_schema = true;
            if (layout.schema_version > supported_schema) {
                // Uno schema piu' recente non si interpreta: si rifiuta.
                // Indovinare che cosa siano i campi nuovi produrrebbe una
                // disposizione plausibile e sbagliata.
                return fail(ParseProblem::unsupported_schema, key);
            }
        } else if (key == "revision") {
            double value = 0.0;
            if (!reader.number(value)) return fail(ParseProblem::wrong_type, key);
            layout.revision = static_cast<int>(value);
        } else if (key == "profile_id") {
            if (!reader.string(layout.profile_id)) {
                return fail(ParseProblem::wrong_type, key);
            }
        } else if (key == "capability") {
            std::string value;
            if (!reader.string(value)) return fail(ParseProblem::wrong_type, key);
            if (value == "professional") {
                layout.capability = Capability::professional;
            } else if (value == "standard") {
                layout.capability = Capability::standard;
            } else {
                return fail(ParseProblem::wrong_type, key);
            }
        } else if (key == "pages") {
            if (!reader.consume('[')) return fail(ParseProblem::wrong_type, key);
            while (!reader.peek(']')) {
                PanelPage page;
                if (!read_page(reader, page)) return fail(ParseProblem::malformed, "pages");
                layout.pages.push_back(page);
                if (!reader.consume(',')) break;
            }
            if (!reader.consume(']')) return fail(ParseProblem::malformed, "pages");
        } else {
            // Chiave sconosciuta. Ignorarla sarebbe comodo e sbagliato: se
            // qualcuno l'ha scritta si aspetta che conti.
            static_cast<void>(reader.skip_value());
            return fail(ParseProblem::malformed, key);
        }

        if (!reader.consume(',')) break;
    }

    if (!reader.consume('}')) return fail(ParseProblem::malformed);
    if (!have_schema) return fail(ParseProblem::missing_field, "schema_version");
    if (layout.pages.empty()) return fail(ParseProblem::missing_field, "pages");

    ParseResult result;
    result.layout = std::move(layout);
    return result;
}

PanelStore::PanelStore(PanelPaths paths) : paths_(std::move(paths)) {}

void PanelStore::record(const std::string& action, const std::string& outcome,
                        const std::string& detail) const {
    if (paths_.audit.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(paths_.audit.parent_path(), error);
    std::ofstream log{paths_.audit, std::ios::app};
    if (!log) return;
    log << action << '\t' << outcome << '\t' << detail << '\n';
}

ParseResult PanelStore::load() const {
    std::ifstream file{paths_.active};
    if (!file) return fail(ParseProblem::missing_field, "file");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return from_json(buffer.str());
}

ActivationReport PanelStore::activate(const PanelLayout& layout, const Surface& surface) {
    ActivationReport report;
    report.validation = validate(layout, surface);

    if (!report.validation.accepted()) {
        // Nulla viene scritto. Una disposizione rifiutata non deve poter
        // sostituire una funzionante nemmeno per un istante.
        report.outcome = ActivationOutcome::rejected;
        record("ACTIVATE", activation_text(report.outcome),
               layout_problem_text(report.validation.issues.front().problem));
        return report;
    }

    std::error_code error;
    std::filesystem::create_directories(paths_.active.parent_path(), error);

    if (std::filesystem::exists(paths_.active)) {
        std::filesystem::create_directories(paths_.backups, error);
        const auto backup =
            paths_.backups / ("panel-" + timestamp_suffix() + ".json");
        std::filesystem::copy_file(paths_.active, backup,
                                   std::filesystem::copy_options::overwrite_existing,
                                   error);
        if (error) {
            // Senza copia di sicurezza non si attiva. Attivare senza poter
            // tornare indietro non e' un'opzione per una configurazione critica.
            report.outcome = ActivationOutcome::backup_failed;
            record("ACTIVATE", activation_text(report.outcome), backup.string());
            return report;
        }
        last_backup_ = backup;
        report.backup = backup;
    }

    // Scrittura su file temporaneo e rinomina: un'interruzione a meta' lascia
    // la disposizione precedente intatta invece di un file troncato.
    const auto temporary = paths_.active.string() + ".tmp";
    {
        std::ofstream out{temporary, std::ios::trunc};
        if (!out) {
            report.outcome = ActivationOutcome::write_failed;
            record("ACTIVATE", activation_text(report.outcome), temporary);
            return report;
        }
        out << to_json(layout);
        if (!out) {
            report.outcome = ActivationOutcome::write_failed;
            record("ACTIVATE", activation_text(report.outcome), temporary);
            return report;
        }
    }

    std::filesystem::rename(temporary, paths_.active, error);
    if (error) {
        report.outcome = ActivationOutcome::write_failed;
        record("ACTIVATE", activation_text(report.outcome), paths_.active.string());
        return report;
    }

    report.outcome = ActivationOutcome::activated;
    record("ACTIVATE", activation_text(report.outcome), report.backup.string());
    return report;
}

bool PanelStore::rollback() {
    if (last_backup_.empty() || !std::filesystem::exists(last_backup_)) {
        record("ROLLBACK", "NESSUN_BACKUP", "");
        return false;
    }
    std::error_code error;
    std::filesystem::copy_file(last_backup_, paths_.active,
                               std::filesystem::copy_options::overwrite_existing, error);
    const bool restored = !error;
    record("ROLLBACK", restored ? "RIPRISTINATA" : "FALLITO", last_backup_.string());
    return restored;
}

}  // namespace efis::instruments
