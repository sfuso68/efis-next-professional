#pragma once

#include "efis/fdm/airframe.hpp"
#include "efis/fdm/atmosphere.hpp"

#include <array>
#include <cstdint>

namespace efis::fdm {

// Propulsione: dall'aria e dalla manetta alla spinta, al regime, al consumo e
// alle temperature.
//
// Perche' un modello fisico e non una tabella di valori plausibili. La sezione
// motore dell'EFIS esiste per dire al pilota cosa sta facendo il motore, e un
// banco a valori prefissati non la mette alla prova: i numeri salgono e
// scendono quando il copione lo dice, non quando l'aeroplano lo impone. Qui la
// temperatura delle teste sale perche' la potenza e' salita, il regime cala
// perche' l'elica si e' caricata, il consumo scende perche' la quota ha
// ridotto la densita'. Gli allarmi dell'EMS si provano cosi': producendo la
// condizione, non il valore.
//
// Stato: DEVELOPMENT. Le relazioni sono quelle canoniche della propulsione a
// elica; le costanti sono tarate sui valori di targa di un motore di classe
// Rotax 912 e di una turbina di classe PT6, non misurate al banco.

// Guasto del propulsore iniettabile dal banco.
//
// Esistono perche' l'EMS deve poterli mostrare prima che accadano in volo. Si
// iniettano sul modello fisico, non sul valore pubblicato: una pompa
// elettrica guasta fa cadere la pressione del carburante, e da quella cade la
// potenza, e dalla potenza le temperature. Iniettare direttamente il numero
// avrebbe prodotto il valore senza le conseguenze, cioe' proprio il dato
// plausibile ma falso che ADR-004 vuole togliere di mezzo.
enum class PropulsionFault : std::uint8_t {
    none,
    // Perdita della pompa elettrica: la pressione carburante cade sotto il
    // minimo e la potenza disponibile si riduce.
    fuel_pump,
    // Perdita d'olio: la pressione cala e la temperatura sale, nell'ordine in
    // cui succede davvero.
    oil_loss,
    // Alternatore perso: la tensione di sbarra scende a quella della batteria e
    // la corrente va a zero. Il motore continua a girare, che e' il punto:
    // l'EMS deve mostrare un guasto elettrico senza allarmare per il motore.
    alternator,
    // Candela o iniettore di un cilindro: regime e potenza calano di poco, la
    // temperatura di scarico di quel cilindro crolla. E' il guasto che si vede
    // solo guardando i quattro scarichi separati.
    single_cylinder,
    // Spegnimento in volo.
    flameout
};

[[nodiscard]] const char* propulsion_fault_name(PropulsionFault fault) noexcept;

// Stato del propulsore. Unita' SI per le grandezze fisiche, unita' di
// strumento per quelle che l'EMS pubblica cosi' come sono (giri, pollici di
// mercurio, psi, gradi): la conversione avviene una volta sola, qui, e non in
// ogni consumatore.
struct PropulsionState final {
    bool running{};
    bool starter_engaged{};

    double engine_rpm{};
    double propeller_rpm{};
    double shaft_power_kw{};
    double thrust_n{};
    double manifold_pressure_inhg{};
    double fuel_flow_kg_s{};
    double fuel_used_kg{};

    double oil_pressure_psi{};
    double oil_temperature_c{};
    double fuel_pressure_psi{};
    std::array<double, 4> cylinder_head_c{};
    std::array<double, 4> exhaust_gas_c{};

    // Grandezze della turbina. Restano a zero sul motore a pistoni: sono
    // assenti, non nulle, e il consumatore deve poterlo distinguere dal tipo di
    // propulsore dichiarato nella cellula.
    double gas_generator_pct{};
    double interstage_turbine_c{};
    double torque_pct{};

    // Elettrico. Sta qui perche' l'alternatore e' trascinato dal motore: la
    // tensione di sbarra dipende dal fatto che l'albero giri, e separare le due
    // cose richiederebbe di ripetere quella dipendenza altrove.
    double bus_voltage_v{};
    double alternator_current_a{};

    // Rendimento dell'elica e rapporto di funzionamento. Non servono al
    // display, servono a capire il modello quando un numero sorprende.
    double propeller_efficiency{};
    double advance_ratio{};
};

class Propulsion final {
public:
    explicit Propulsion(PropulsionConfig config) noexcept;

    // Avvia il modello a motore spento e freddo, oppure a regime e in
    // temperatura. Il secondo caso serve alle prove che partono in volo e non
    // devono spendere dieci minuti a scaldare l'olio.
    void reset(bool running, double ambient_temperature_c) noexcept;

    void set_fault(PropulsionFault fault) noexcept { fault_ = fault; }
    [[nodiscard]] PropulsionFault fault() const noexcept { return fault_; }

    // Un passo di integrazione. La velocita' vera entra perche' da essa
    // dipendono il carico dell'elica e il rendimento: lo stesso regime produce
    // spinte diverse a velocita' diverse, ed e' il motivo per cui un'elica a
    // passo fisso prende giri in picchiata.
    const PropulsionState& step(const Controls& controls,
                                const AirState& air,
                                double true_airspeed_m_s,
                                double dt_s) noexcept;

    [[nodiscard]] const PropulsionState& state() const noexcept { return state_; }

    // Manetta che produrrebbe la potenza all'albero richiesta nelle condizioni
    // date. Serve a mettere l'aeroplano in volo livellato senza chiedere a chi
    // scrive la prova di indovinare una posizione di manetta: la potenza
    // necessaria si calcola dalla resistenza, e questa la traduce. Non e' un
    // pilota automatico: e' l'inversa statica della curva di potenza.
    [[nodiscard]] double throttle_for_shaft_power(double power_kw,
                                                  const AirState& air) const noexcept;

private:
    [[nodiscard]] double available_power_kw(const Controls& controls,
                                            const AirState& air) const noexcept;
    [[nodiscard]] double absorbed_power_kw(double propeller_rpm,
                                           const AirState& air,
                                           double true_airspeed_m_s,
                                           double pitch_command) const noexcept;
    void update_thermals(const Controls& controls,
                         const AirState& air,
                         double dt_s) noexcept;
    void update_electrical(const Controls& controls, double dt_s) noexcept;

    PropulsionConfig config_;
    PropulsionState state_{};
    PropulsionFault fault_{PropulsionFault::none};

    // Coefficiente di potenza dell'elica al punto di progetto, ricavato dalla
    // condizione che a regime nominale e al livello del mare l'elica assorba
    // esattamente la potenza di targa. Cosi' il modello non ha un parametro
    // libero in piu' da indovinare.
    double power_coefficient_{};

    // Tensione a riposo della batteria. Separata da quella di sbarra perche'
    // sono due cose: la sbarra segue chi la alimenta, la batteria si scarica.
    double battery_voltage_{12.6};
};

}  // namespace efis::fdm
