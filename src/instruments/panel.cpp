#include "efis/instruments/panel.hpp"

#include "efis/core/signal_access.hpp"

#include <algorithm>
#include <array>

namespace efis::instruments {
namespace {

// Grandezze richieste da ciascuno strumento. Sono elencate per esteso perche'
// il menu di scelta le usa per sapere che cosa puo' offrire: uno strumento la
// cui sorgente non esiste non deve comparire, non deve comparire e poi mostrare
// la bandierina.
constexpr SignalId airspeed_needs[] = {SignalId::indicated_airspeed};
constexpr SignalId altimeter_needs[] = {SignalId::indicated_altitude,
                                        SignalId::baro_setting};
constexpr SignalId vertical_speed_needs[] = {SignalId::vertical_speed};
constexpr SignalId attitude_needs[] = {SignalId::roll, SignalId::pitch};
constexpr SignalId heading_needs[] = {SignalId::heading};
constexpr SignalId turn_needs[] = {SignalId::turn_rate, SignalId::lateral_acceleration};

constexpr SignalId tachometer_needs[] = {SignalId::engine_rpm};
constexpr SignalId manifold_needs[] = {SignalId::manifold_pressure};
constexpr SignalId oil_pressure_needs[] = {SignalId::oil_pressure};
constexpr SignalId oil_temperature_needs[] = {SignalId::oil_temperature};
constexpr SignalId fuel_pressure_needs[] = {SignalId::fuel_pressure};
constexpr SignalId fuel_flow_needs[] = {SignalId::fuel_flow};
constexpr SignalId fuel_quantity_needs[] = {SignalId::fuel_quantity_left,
                                            SignalId::fuel_quantity_right};
constexpr SignalId voltage_needs[] = {SignalId::bus_voltage};
constexpr SignalId current_needs[] = {SignalId::alternator_current};
constexpr SignalId cht_needs[] = {SignalId::cylinder_head_temperature_1,
                                  SignalId::cylinder_head_temperature_2,
                                  SignalId::cylinder_head_temperature_3,
                                  SignalId::cylinder_head_temperature_4};
constexpr SignalId egt_needs[] = {SignalId::exhaust_gas_temperature_1,
                                  SignalId::exhaust_gas_temperature_2,
                                  SignalId::exhaust_gas_temperature_3,
                                  SignalId::exhaust_gas_temperature_4};

constexpr InstrumentDefinition catalogue[] = {
    // La sei-strumenti di base e' disponibile anche in Standard: i display
    // piccoli e circolari sono proprio la sua fascia, ed e' li' che una
    // presentazione a quadranti serve di piu'.
    {InstrumentKind::airspeed, "Anemometro", airspeed_needs, Capability::standard},
    {InstrumentKind::altimeter, "Altimetro", altimeter_needs, Capability::standard},
    {InstrumentKind::vertical_speed, "Variometro", vertical_speed_needs,
     Capability::standard},
    {InstrumentKind::attitude, "Orizzonte artificiale", attitude_needs,
     Capability::standard},
    {InstrumentKind::heading, "Girodirezionale", heading_needs, Capability::standard},
    {InstrumentKind::turn_and_slip, "Virosbandometro", turn_needs, Capability::standard},

    // Gli strumenti motore completi richiedono la catena EMS, che e' la
    // configurazione Professional.
    {InstrumentKind::tachometer, "Contagiri", tachometer_needs, Capability::standard},
    {InstrumentKind::manifold_pressure, "Pressione di alimentazione", manifold_needs,
     Capability::professional},
    {InstrumentKind::oil_pressure, "Pressione olio", oil_pressure_needs,
     Capability::standard},
    {InstrumentKind::oil_temperature, "Temperatura olio", oil_temperature_needs,
     Capability::standard},
    {InstrumentKind::fuel_pressure, "Pressione carburante", fuel_pressure_needs,
     Capability::professional},
    {InstrumentKind::fuel_flow, "Flusso carburante", fuel_flow_needs,
     Capability::professional},
    {InstrumentKind::fuel_quantity, "Quantita' carburante", fuel_quantity_needs,
     Capability::standard},
    {InstrumentKind::bus_voltage, "Tensione", voltage_needs, Capability::standard},
    {InstrumentKind::alternator_current, "Corrente", current_needs,
     Capability::professional},
    {InstrumentKind::cylinder_head_temperature, "Temperatura teste", cht_needs,
     Capability::professional},
    {InstrumentKind::exhaust_gas_temperature, "Temperatura gas di scarico", egt_needs,
     Capability::professional},
};

bool signal_exists(SignalId signal) {
    const auto flight = flight_signals();
    if (std::find(flight.begin(), flight.end(), signal) != flight.end()) return true;
    const auto engine = engine_signals();
    return std::find(engine.begin(), engine.end(), signal) != engine.end();
}

}  // namespace

std::span<const InstrumentDefinition> instrument_catalogue() noexcept {
    return catalogue;
}

const InstrumentDefinition* find_instrument(InstrumentKind kind) noexcept {
    for (const InstrumentDefinition& entry : catalogue) {
        if (entry.kind == kind) return &entry;
    }
    return nullptr;
}

ValidationResult validate(const PanelLayout& layout, const Surface& surface) {
    ValidationResult result;
    auto report = [&result](LayoutProblem problem, int page = -1, int row = -1,
                            int column = -1) {
        result.issues.push_back(LayoutIssue{problem, page, row, column});
    };

    bool any_slot = false;
    for (std::size_t index = 0; index < layout.pages.size(); ++index) {
        const PanelPage& page = layout.pages[index];
        const int page_number = static_cast<int>(index);

        if (page.rows <= 0 || page.columns <= 0) {
            report(LayoutProblem::empty_grid, page_number);
            continue;
        }

        // La dimensione del quadrante la detta la direzione piu' stretta: sei
        // strumenti su una pagina larga restano quadrati, e a decidere e'
        // l'altezza.
        const double cell = std::min(surface.width_px / page.columns,
                                     surface.height_px / page.rows);
        if (cell < surface.minimum_cell_px) {
            report(LayoutProblem::illegible_at_resolution, page_number);
        }

        for (const Slot& slot : page.slots) {
            any_slot = true;

            if (slot.row < 0 || slot.row >= page.rows || slot.column < 0 ||
                slot.column >= page.columns) {
                report(LayoutProblem::slot_outside_grid, page_number, slot.row,
                       slot.column);
                continue;
            }

            const std::size_t same = static_cast<std::size_t>(
                std::count_if(page.slots.begin(), page.slots.end(),
                              [&slot](const Slot& other) {
                                  return other.row == slot.row &&
                                         other.column == slot.column;
                              }));
            if (same > 1) {
                report(LayoutProblem::overlapping_slots, page_number, slot.row,
                       slot.column);
            }

            const InstrumentDefinition* definition = find_instrument(slot.kind);
            if (definition == nullptr) {
                report(LayoutProblem::unknown_instrument, page_number, slot.row,
                       slot.column);
                continue;
            }

            if (definition->capability == Capability::professional &&
                layout.capability == Capability::standard) {
                report(LayoutProblem::capability_required, page_number, slot.row,
                       slot.column);
            }

            for (const SignalId signal : definition->required) {
                if (!signal_exists(signal)) {
                    report(LayoutProblem::signal_unavailable, page_number, slot.row,
                           slot.column);
                    break;
                }
            }
        }
    }

    if (!any_slot) report(LayoutProblem::empty_layout);
    return result;
}

PanelLayout classic_six_pack() {
    // La disposizione su cui ogni pilota ha imparato a spostare lo sguardo.
    // Cambiarla renderebbe piu' lenta proprio la scansione che deve essere
    // automatica.
    PanelPage page;
    page.name = "Sei strumenti";
    page.rows = 2;
    page.columns = 3;
    page.slots = {
        {0, 0, InstrumentKind::airspeed},
        {0, 1, InstrumentKind::attitude},
        {0, 2, InstrumentKind::altimeter},
        {1, 0, InstrumentKind::turn_and_slip},
        {1, 1, InstrumentKind::heading},
        {1, 2, InstrumentKind::vertical_speed},
    };

    PanelLayout layout;
    layout.pages.push_back(page);
    return layout;
}

PanelLayout engine_page() {
    PanelPage page;
    page.name = "Motore";
    page.rows = 2;
    page.columns = 3;
    page.slots = {
        {0, 0, InstrumentKind::tachometer},
        {0, 1, InstrumentKind::oil_pressure},
        {0, 2, InstrumentKind::oil_temperature},
        {1, 0, InstrumentKind::fuel_quantity},
        {1, 1, InstrumentKind::cylinder_head_temperature},
        {1, 2, InstrumentKind::bus_voltage},
    };

    PanelLayout layout;
    layout.capability = Capability::professional;
    layout.pages.push_back(page);
    return layout;
}

}  // namespace efis::instruments
