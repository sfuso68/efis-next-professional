#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/source_arbitrator.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace efis {

// Lo stato prodotto da una sorgente, con l'identita' della sorgente stessa.
//
// Il campo `source` non e' ridondante rispetto a quello dei campioni: descrive
// chi ha prodotto lo snapshot, mentre i campioni possono restare non
// valorizzati. Una sorgente che non pubblica una certa grandezza deve poter
// essere distinta da una sorgente assente.
struct SnapshotContribution final {
    SourceId source{SourceId::unknown};
    FlightSnapshot snapshot{};
};

struct ArbitrationStatistics final {
    // Passaggi a una sorgente diversa perche' quella attiva non era piu'
    // utilizzabile.
    std::uint64_t failovers{};

    // Ritorni a una sorgente di priorita' superiore, dopo il tempo di
    // permanenza previsto.
    std::uint64_t reversions{};

    // Campioni utilizzabili che dichiarano un segnale diverso da quello del
    // campo in cui si trovano.
    //
    // E' un difetto di chi li produce, non una condizione operativa: significa
    // che una grandezza e' finita nel posto sbagliato. Viene contato invece che
    // corretto, perche' riscrivere l'etichetta nasconderebbe l'errore e
    // renderebbe la registrazione una descrizione falsa di cio' che e'
    // successo.
    std::uint64_t mislabelled_samples{};
};

// Arbitra fra piu' sorgenti e produce un solo stato di volo.
//
// L'arbitrato e' **per grandezza**, non per sorgente. Non e' un dettaglio
// implementativo: significa che la posizione puo' venire dal Pixhawk mentre
// l'assetto resta simulato, che e' precisamente lo scenario di guasto parziale.
// Scegliere una sorgente in blocco costringerebbe a scartare grandezze ancora
// buone insieme a quella caduta.
//
// I contributi vanno passati in ordine di priorita': il primo e' il preferito.
// La perdita della sorgente attiva provoca failover immediato verso la prima
// utilizzabile; il ritorno a una sorgente di priorita' superiore richiede il
// tempo di permanenza della politica. Vedi ADR-005.
//
// L'arbitro non invecchia i campioni e non calcola allarmi: si colloca prima di
// FlightModel::normalize, che di quello si occupa gia'.
class SnapshotArbitrator final {
public:
    explicit SnapshotArbitrator(ArbitrationPolicy policy = {});

    [[nodiscard]] FlightSnapshot combine(
        std::span<const SnapshotContribution> contributions, MonoTime now);

    // Cambi di sorgente accumulati, destinati alla black box. Il paragrafo 17.1
    // dell'handoff li vuole registrati: sapere quando una grandezza ha cambiato
    // origine e' necessario per rileggere un volo.
    [[nodiscard]] const std::vector<SourceChangeEvent>& events() const noexcept {
        return events_;
    }
    void clear_events() noexcept { events_.clear(); }

    [[nodiscard]] const ArbitrationStatistics& statistics() const noexcept {
        return statistics_;
    }

    // Sorgente attualmente selezionata per una grandezza. Restituisce
    // SourceId::unknown se la grandezza non e' arbitrata o non e' mai stata
    // servita da nessuno.
    [[nodiscard]] SourceId selected_for(SignalId signal) const noexcept;

private:
    ArbitrationPolicy policy_;
    std::vector<SourceArbitrator<double>> numeric_;
    std::vector<SourceArbitrator<bool>> discrete_;
    std::vector<SourceChangeEvent> events_;
    ArbitrationStatistics statistics_{};
    std::uint64_t sequence_{};
};

}  // namespace efis
