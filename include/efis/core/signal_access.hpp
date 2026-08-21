#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/engine/engine_snapshot.hpp"

#include <span>

namespace efis {

// Accesso a una grandezza per identificativo, invece che per nome di campo.
//
// Il renderer espone oggi una proprieta' scalare per grandezza, decine in
// tutto. Finche' gli strumenti li decide chi scrive il codice quel modello
// regge; con strumenti scelti dal pilota no, perche' ogni strumento nuovo
// richiederebbe codice nuovo. Un pannello configurabile deve poter chiedere
// «dammi la grandezza 201» senza che nessuno abbia previsto quella domanda.
//
// Sta nel core e non nel renderer per due motivi: e' verificabile senza Qt, e
// la stessa corrispondenza serve gia' all'arbitrato e servira' alla black box.
//
// Restituisce un puntatore nullo quando la grandezza non esiste in quello
// stato. Nullo significa «questo stato non porta quella grandezza», che e'
// diverso da «la porta e non e' valida»: la seconda condizione sta nel campione,
// e chi la confonde con la prima finisce per mostrare un quadrante spento al
// posto di uno guasto.

[[nodiscard]] const Sample<double>* find_signal(const FlightSnapshot& snapshot,
                                                SignalId signal) noexcept;

[[nodiscard]] const Sample<double>* find_signal(const engine::EngineSnapshot& snapshot,
                                                SignalId signal) noexcept;

// Grandezze effettivamente ottenibili dai due stati.
//
// Servono al menu di scelta degli strumenti: un menu che offrisse uno strumento
// per una grandezza senza sorgente sarebbe una promessa che il pannello non puo'
// mantenere, e il pilota lo scoprirebbe in volo davanti a un quadrante con la
// bandierina alzata.
//
// L'elenco dice quali grandezze il sistema sa produrre, non quali siano valide
// adesso: quello lo dice il campione.
[[nodiscard]] std::span<const SignalId> flight_signals() noexcept;
[[nodiscard]] std::span<const SignalId> engine_signals() noexcept;

}  // namespace efis
