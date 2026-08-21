#pragma once

#include "efis/core/types.hpp"

#include <span>
#include <string>
#include <vector>

namespace efis::instruments {

// Pannello analogico configurabile dal pilota.
//
// Il modello sta nel core e non nel renderer per la ragione di sempre: e'
// verificabile senza Qt, e le regole che decidono se una disposizione sia
// accettabile sono regole di sicurezza, non di presentazione.
//
// Una pagina che promette uno strumento senza sorgente, o che schiaccia sei
// quadranti in uno spazio in cui non si leggono, non e' un problema estetico. Il
// pilota lo scoprirebbe in volo, davanti a un quadrante con la bandierina
// alzata o a una scala che non riesce a leggere.

// Tipo di strumento collocabile su una pagina.
//
// Come per SignalId, gli identificativi finiscono in un file di configurazione
// che sopravvive agli aggiornamenti: ogni famiglia parte da una base esplicita e
// lascia spazio, cosi' aggiungere uno strumento non sposta le pagine gia'
// salvate dal pilota.
enum class InstrumentKind : std::uint16_t {
    unknown = 0,

    // Volo
    airspeed = 100,
    altimeter,
    vertical_speed,
    attitude,
    heading,
    turn_and_slip,

    // Motore
    tachometer = 200,
    manifold_pressure,
    oil_pressure,
    oil_temperature,
    fuel_pressure,
    fuel_flow,
    fuel_quantity,
    bus_voltage,
    alternator_current,
    cylinder_head_temperature,
    exhaust_gas_temperature
};

// Profilo in cui lo strumento e' disponibile.
//
// Il paragrafo 18 dell'handoff vuole una sola codebase con differenze gestite
// per capability, non per fork. Standard riceve la sei-strumenti di base;
// Professional anche navigazione e motore completo.
enum class Capability : std::uint8_t { standard, professional };

// Cosa serve a uno strumento per esistere.
struct InstrumentDefinition final {
    InstrumentKind kind;

    // Nome per il menu di scelta.
    const char* name;

    // Grandezze senza le quali lo strumento non ha nulla da mostrare. Se una
    // manca, offrirlo nel menu sarebbe una promessa che il pannello non puo'
    // mantenere.
    std::span<const SignalId> required;

    Capability capability;
};

[[nodiscard]] std::span<const InstrumentDefinition> instrument_catalogue() noexcept;
[[nodiscard]] const InstrumentDefinition* find_instrument(InstrumentKind kind) noexcept;

// Posizione di uno strumento su una pagina.
struct Slot final {
    int row{};
    int column{};
    InstrumentKind kind{InstrumentKind::unknown};
};

struct PanelPage final {
    std::string name;
    int rows{2};
    int columns{3};
    std::vector<Slot> slots;
};

struct PanelLayout final {
    int schema_version{1};
    int revision{1};

    // Profilo aeromobile per cui la disposizione e' stata salvata. Una pagina
    // costruita per un aeromobile non vale automaticamente per un altro: le
    // scale e i limiti cambiano.
    std::string profile_id;

    Capability capability{Capability::standard};
    std::vector<PanelPage> pages;
};

// Perche' una disposizione e' stata rifiutata.
//
// Ogni motivo e' un guasto diverso e va detto per esteso: «configurazione non
// valida» non permette a nessuno di correggerla.
enum class LayoutProblem : std::uint8_t {
    none,

    // Due strumenti nella stessa posizione: uno coprirebbe l'altro.
    overlapping_slots,

    // Strumento fuori dalla griglia dichiarata.
    slot_outside_grid,

    // Griglia senza righe o senza colonne.
    empty_grid,

    // Tipo di strumento sconosciuto: viene da una versione piu' recente, oppure
    // il file e' stato modificato a mano.
    unknown_instrument,

    // Lo strumento richiede grandezze che questo sistema non produce. Offrirlo
    // sarebbe una promessa non mantenibile.
    signal_unavailable,

    // Lo strumento richiede il profilo Professional.
    capability_required,

    // I quadranti risulterebbero piu' piccoli della soglia di leggibilita'.
    illegible_at_resolution,

    // Nessuna pagina, o una pagina senza strumenti.
    empty_layout
};

[[nodiscard]] constexpr const char* layout_problem_text(LayoutProblem problem) noexcept {
    switch (problem) {
        case LayoutProblem::none: return "OK";
        case LayoutProblem::overlapping_slots: return "SLOT_SOVRAPPOSTI";
        case LayoutProblem::slot_outside_grid: return "SLOT_FUORI_GRIGLIA";
        case LayoutProblem::empty_grid: return "GRIGLIA_VUOTA";
        case LayoutProblem::unknown_instrument: return "STRUMENTO_SCONOSCIUTO";
        case LayoutProblem::signal_unavailable: return "GRANDEZZA_ASSENTE";
        case LayoutProblem::capability_required: return "PROFILO_INSUFFICIENTE";
        case LayoutProblem::illegible_at_resolution: return "ILLEGGIBILE";
        case LayoutProblem::empty_layout: return "DISPOSIZIONE_VUOTA";
    }
    return "SLOT_SOVRAPPOSTI";
}

struct LayoutIssue final {
    LayoutProblem problem{LayoutProblem::none};

    // Pagina e posizione a cui il problema si riferisce, per poterlo indicare
    // invece di lasciarlo cercare. Pagina negativa quando riguarda l'insieme.
    int page{-1};
    int row{-1};
    int column{-1};
};

struct ValidationResult final {
    std::vector<LayoutIssue> issues;

    [[nodiscard]] bool accepted() const noexcept { return issues.empty(); }
};

// Dimensione fisica della superficie su cui la pagina verra' disegnata.
struct Surface final {
    double width_px{1024.0};
    double height_px{600.0};

    // Diametro sotto il quale un quadrante non e' piu' leggibile.
    //
    // Duecentoquaranta pixel non e' un numero scelto a tavolino: e' la
    // dimensione piu' piccola alla quale gli strumenti sono stati effettivamente
    // guardati, quella delle immagini di riferimento a 240 px, ed e' anche il
    // limite inferiore della classe Round del design system. Sotto non e' che
    // diventino brutti: le tacche si diradano fino a lasciare una scala che non
    // si legge piu' a colpo d occhio, che e' il momento in cui uno strumento
    // smette di servire.
    double minimum_cell_px{240.0};
};

// Verifica una disposizione. Non la corregge: correggerla in silenzio
// nasconderebbe l'errore a chi l'ha scritta, e il paragrafo 22 dell'handoff lo
// vieta.
[[nodiscard]] ValidationResult validate(const PanelLayout& layout,
                                        const Surface& surface);

// Disposizioni classiche di partenza.
//
// La decisione presa con il proprietario e' modelli canonici con slot
// sostituibili, non griglia libera: si parte da una disposizione geometricamente
// corretta e si cambia uno strumento alla volta, invece di poter costruire da
// zero qualcosa di illeggibile.
[[nodiscard]] PanelLayout classic_six_pack();
[[nodiscard]] PanelLayout engine_page();

}  // namespace efis::instruments
