#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/snapshot_arbitrator.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace efis {

// Una sorgente di stato di volo.
//
// Non dichiara da dove prenda i dati: puo' essere una simulazione, un
// collegamento di rete verso il nodo di ingestion, o un banco prova. Il display
// che la consuma non deve saperlo, ed e' il motivo per cui questa interfaccia
// esiste invece di un provider concreto scritto nel controller. Vedi ADR-003.
class FlightSource {
public:
    FlightSource() = default;
    FlightSource(const FlightSource&) = delete;
    FlightSource& operator=(const FlightSource&) = delete;
    FlightSource(FlightSource&&) = delete;
    FlightSource& operator=(FlightSource&&) = delete;
    virtual ~FlightSource() = default;

    [[nodiscard]] virtual SourceId source() const noexcept = 0;

    // Stato corrente secondo questa sorgente. `elapsed` e' il tempo dall'avvio,
    // che serve alle sorgenti generate; quelle reali possono ignorarlo.
    [[nodiscard]] virtual FlightSnapshot poll(MonoTime now,
                                              std::chrono::milliseconds elapsed) = 0;
};

// Ruolo della simulazione nell'arbitrato.
//
// La simulazione non e' un residuo da rimuovere quando arriveranno i sensori
// veri: e' cio' che permette di lavorare al banco quando i sensori non ci sono,
// sono in parte collegati, o si stanno tarando. Deve pero' essere una scelta
// dichiarata, non un comportamento implicito.
enum class SimulationRole : std::uint8_t {
    // Nessun dato simulato entra nell'arbitrato. Cio' che i sensori reali non
    // forniscono resta indisponibile, ed e' l'unica configurazione onesta per
    // una prova che debba dire qualcosa sul comportamento reale.
    disabled,

    // La simulazione copre soltanto cio' che le sorgenti reali non forniscono.
    // E' il valore normale allo sviluppo: l'assetto vero resta vero, e le
    // grandezze mancanti non lasciano il quadro vuoto.
    fallback,

    // La simulazione ha la precedenza sulle sorgenti reali. Serve a riprodurre
    // uno scenario deterministico avendo comunque i sensori collegati, per
    // esempio per confrontare a posteriori cio' che il sensore misurava con cio'
    // che lo scenario prevedeva.
    primary
};

[[nodiscard]] constexpr const char* simulation_role_name(SimulationRole role) noexcept {
    switch (role) {
        case SimulationRole::disabled: return "DISABLED";
        case SimulationRole::fallback: return "FALLBACK";
        case SimulationRole::primary: return "PRIMARY";
    }
    return "DISABLED";
}

// Interroga le sorgenti registrate e produce un solo stato arbitrato.
//
// L'ordine di priorita' non e' quello di registrazione ma quello che il ruolo
// della simulazione impone: e' una decisione avionica e sta nel core, non nel
// controller Qt ne' tantomeno nel QML, come stabilito dalla ADR-002.
class FlightSourceRouter final {
public:
    explicit FlightSourceRouter(ArbitrationPolicy policy = {});

    // Registra una sorgente reale, in ordine di priorita' decrescente.
    void add_source(std::shared_ptr<FlightSource> source);

    // Registra la sorgente simulata. Ce n'e' una sola, e il suo posto
    // nell'arbitrato lo decide il ruolo.
    void set_simulation(std::shared_ptr<FlightSource> simulation);

    // Cambiare ruolo riazzera l'arbitrato: e' una riconfigurazione voluta, non
    // una sorgente che si riprende, e deve avere effetto subito.
    void set_simulation_role(SimulationRole role) noexcept {
        if (role == role_) return;
        role_ = role;
        arbitrator_.reset();
    }
    [[nodiscard]] SimulationRole simulation_role() const noexcept { return role_; }

    // Interroga le sorgenti e restituisce lo stato arbitrato, non ancora
    // normalizzato: invecchiamento, salute e allarmi restano compito di
    // FlightModel, che viene dopo.
    [[nodiscard]] FlightSnapshot poll(MonoTime now, std::chrono::milliseconds elapsed);

    // Ultimo contributo prodotto dalla simulazione, prima dell'arbitrato. Serve
    // agli strumenti di sviluppo che iniettano guasti sul solo dato simulato.
    [[nodiscard]] FlightSnapshot& simulated_contribution() noexcept {
        return simulated_;
    }

    [[nodiscard]] const SnapshotArbitrator& arbitrator() const noexcept {
        return arbitrator_;
    }
    [[nodiscard]] SnapshotArbitrator& arbitrator() noexcept { return arbitrator_; }

    [[nodiscard]] std::size_t source_count() const noexcept { return sources_.size(); }

private:
    SnapshotArbitrator arbitrator_;
    std::vector<std::shared_ptr<FlightSource>> sources_;
    std::shared_ptr<FlightSource> simulation_;
    SimulationRole role_{SimulationRole::fallback};
    FlightSnapshot simulated_{};
    std::vector<SnapshotContribution> contributions_;
};

}  // namespace efis
