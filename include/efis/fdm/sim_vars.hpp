#pragma once

#include "efis/fdm/dynamics.hpp"
#include "efis/fdm/propulsion.hpp"
#include "efis/fdm/sensors.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace efis::fdm {

// Bus delle variabili di simulazione.
//
// Questa e' la parte del lavoro che viene dall'analisi di Flight Simulator 2024
// e non dalla meccanica del volo. Il risultato di quell'analisi, per quanto
// riguarda l'interfaccia, e' semplice e vale la pena di scriverlo: il
// simulatore non espone un modello dati, espone un dizionario. Una variabile si
// chiede per nome e per unita', il motore la converte, e chi la chiede non sa
// nulla di come sia calcolata. I nomi sono stringhe con spazi, il suffisso
// ":1" seleziona l'indice (il primo motore, la prima sbarra), e l'unita' fa
// parte della richiesta e non della variabile.
//
// Adottare quella forma qui, invece di inventarne una, ha una conseguenza
// pratica precisa: lo stesso consumatore puo' essere alimentato da questo banco
// su Linux oppure da Flight Simulator attraverso SimConnect, senza modifiche.
// Il banco e il simulatore commerciale diventano due sorgenti della stessa
// interfaccia, e la scelta fra i due e' una questione di quale macchina si ha
// sotto mano.
//
// Dove il banco si discosta deliberatamente, e perche'. In Flight Simulator le
// variabili sono la verita' del modello: non esiste uno strato di sensori, e la
// velocita' indicata e' la velocita' calibrata corretta per un errore di
// posizione dichiarato nella configurazione, non una misura con rumore,
// ritardo e possibilita' di guasto. Un EFIS provato contro quelle variabili
// non puo' essere messo alla prova su cio' che ADR-004 chiede: accorgersi di
// un sensore che mente. Qui quindi i nomi compatibili portano il valore
// misurato, cioe' cio' che un avionico vedrebbe davvero, e la verita' del
// modello viene pubblicata a parte, sotto nomi che cominciano con "BENCH
// TRUTH" e che nessun simulatore commerciale ha. La verita' non e' per il
// display: e' per chi giudica il display.

// Provenienza del nome, che e' un'informazione di verificabilita' e non di
// gusto: un nome confermato si puo' controllare aprendo l'SDK, uno di
// documentazione no.
enum class NameOrigin : std::uint8_t {
    // Nome trovato nei file ModelBehaviorDefs dell'SDK installato, dove i
    // comportamenti di Asobo lo usano per leggere lo stato del simulatore.
    confirmed_in_sdk,
    // Nome che appartiene all'elenco documentato delle variabili di
    // simulazione ma che non compare in quei file.
    documented,
    // Nome del banco, che in Flight Simulator non esiste.
    bench_extension
};

struct SimVarSample final {
    std::string_view name;
    std::string_view unit;
    double value{};

    // Falso quando la grandezza non e' disponibile in questo istante: nessun
    // fix satellitare, nessuna turbina su un motore a pistoni. Non e' lo zero:
    // lo zero e' un valore, e un consumatore che non distinguesse i due casi
    // mostrerebbe una posizione all'equatore a chi ha perso il segnale.
    bool available{true};

    NameOrigin origin{NameOrigin::documented};
};

// Tutto cio' che serve a comporre il bus. Riferimenti e non copie: il bus si
// compone a ogni fotogramma, e copiare lo stato a ogni fotogramma sarebbe un
// costo pagato per nulla.
struct BusInputs final {
    const VehicleState& truth;
    const IndicatedState& indicated;
    const PropulsionState& engine;
    const Airframe& airframe;
    const Environment& environment;
    const Controls& controls;
};

// Compone il bus nel vettore dato, che viene svuotato e riempito. Il vettore e'
// del chiamante perche' cosi' la sua memoria si riusa fra i fotogrammi: a
// cinquanta hertz una allocazione per fotogramma non si nota, a mille no.
void publish_sim_vars(const BusInputs& inputs, std::vector<SimVarSample>& out);

// Cerca una variabile per nome. Restituisce il valore solo se la variabile
// esiste ed e' disponibile: due assenze diverse per il chiamante sono la stessa
// cosa, e per il display anche.
[[nodiscard]] std::optional<double> find_sim_var(const std::vector<SimVarSample>& bus,
                                                 std::string_view name) noexcept;

[[nodiscard]] const char* name_origin_label(NameOrigin origin) noexcept;

}  // namespace efis::fdm
