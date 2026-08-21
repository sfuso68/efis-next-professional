#pragma once

#include "efis/instruments/panel.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace efis::instruments {

// Persistenza delle disposizioni del pannello.
//
// Segue la stessa disciplina del profilo aeromobile, che il paragrafo 5
// dell'handoff impone a ogni configurazione critica: si mette in attesa, si
// valida, si attiva in modo atomico lasciando una copia di sicurezza e una riga
// di registro, e si puo' tornare indietro.
//
// Sta nel core e non nel renderer per due motivi. Il primo e' che si puo'
// provare senza Qt, e una procedura di attivazione che non si puo' provare non
// da' alcuna garanzia. Il secondo e' che la validazione, che e' la parte che
// conta, e' gia' qui.
//
// Il formato e' JSON come le altre configurazioni del progetto, ma il lettore e'
// scritto per questo schema soltanto ed e' deliberatamente severo: una chiave
// sconosciuta, un tipo sbagliato o un campo mancante fanno rifiutare il file
// invece di produrre una disposizione parziale. Un pannello che si apre a meta'
// e' peggio di uno che non si apre: il pilota non sa che cosa manchi.

[[nodiscard]] std::string to_json(const PanelLayout& layout);

// Perche' un file non e' stato accettato.
enum class ParseProblem : std::uint8_t {
    none,
    // Il testo non e' JSON, o non nella forma che questo schema prevede.
    malformed,
    // Manca un campo obbligatorio.
    missing_field,
    // Un campo c'e' ma non e' del tipo previsto.
    wrong_type,
    // Lo schema del file e' piu' recente di quello che questo programma sa
    // leggere. Interpretarlo comunque significherebbe indovinare.
    unsupported_schema
};

[[nodiscard]] constexpr const char* parse_problem_text(ParseProblem problem) noexcept {
    switch (problem) {
        case ParseProblem::none: return "OK";
        case ParseProblem::malformed: return "FORMATO_NON_VALIDO";
        case ParseProblem::missing_field: return "CAMPO_MANCANTE";
        case ParseProblem::wrong_type: return "TIPO_ERRATO";
        case ParseProblem::unsupported_schema: return "SCHEMA_NON_SUPPORTATO";
    }
    return "FORMATO_NON_VALIDO";
}

struct ParseResult final {
    std::optional<PanelLayout> layout;
    ParseProblem problem{ParseProblem::none};

    // Nome del campo su cui la lettura si e' fermata, quando si sa.
    std::string field;

    [[nodiscard]] bool ok() const noexcept { return layout.has_value(); }
};

[[nodiscard]] ParseResult from_json(const std::string& text);

// Esito di un tentativo di attivazione.
enum class ActivationOutcome : std::uint8_t {
    // Attivata, con copia di sicurezza della precedente.
    activated,
    // Rifiutata dalla validazione: nulla e' stato scritto.
    rejected,
    // La copia di sicurezza non e' riuscita, quindi non si e' scritto nulla.
    // Attivare senza poter tornare indietro non e' un'opzione.
    backup_failed,
    // La scrittura non e' riuscita. La precedente resta al suo posto.
    write_failed
};

[[nodiscard]] constexpr const char* activation_text(ActivationOutcome outcome) noexcept {
    switch (outcome) {
        case ActivationOutcome::activated: return "ATTIVATA";
        case ActivationOutcome::rejected: return "RIFIUTATA";
        case ActivationOutcome::backup_failed: return "BACKUP_FALLITO";
        case ActivationOutcome::write_failed: return "SCRITTURA_FALLITA";
    }
    return "SCRITTURA_FALLITA";
}

struct ActivationReport final {
    ActivationOutcome outcome{ActivationOutcome::rejected};
    ValidationResult validation;
    std::filesystem::path backup;
};

// Percorsi su cui la persistenza lavora. Sono espliciti invece che dedotti,
// cosi' le prove possono usare una directory temporanea senza toccare la
// configurazione vera.
struct PanelPaths final {
    std::filesystem::path active;
    std::filesystem::path backups;
    std::filesystem::path audit;
};

class PanelStore final {
public:
    explicit PanelStore(PanelPaths paths);

    // Legge la disposizione attiva. Assente se il file non c'e' o non e'
    // leggibile: un pannello che si apre a meta' e' peggio di uno che non si
    // apre.
    [[nodiscard]] ParseResult load() const;

    // Valida e attiva. La scrittura passa da un file temporaneo rinominato:
    // un'interruzione a meta' lascia la disposizione precedente intatta invece
    // di un file troncato.
    [[nodiscard]] ActivationReport activate(const PanelLayout& layout,
                                            const Surface& surface);

    // Ripristina l'ultima copia di sicurezza di questa sessione.
    [[nodiscard]] bool rollback();

    [[nodiscard]] const std::filesystem::path& last_backup() const noexcept {
        return last_backup_;
    }

private:
    void record(const std::string& action, const std::string& outcome,
                const std::string& detail) const;

    PanelPaths paths_;
    std::filesystem::path last_backup_;
};

}  // namespace efis::instruments
