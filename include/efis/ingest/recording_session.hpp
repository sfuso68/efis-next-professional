#pragma once

#include "efis/ingest/mavlink_pipeline.hpp"
#include "efis/io/byte_source.hpp"
#include "efis/recording/segmented_recorder.hpp"

#include <cstdint>
#include <vector>

namespace efis::ingest {

struct RecordingSessionStatistics final {
    std::uint64_t bytes_read{};
    std::uint64_t snapshots_recorded{};

    // Eventi scritti nel registro. Sono contati separatamente dai campioni
    // perche' rispondono a domande diverse: i campioni dicono quali valori sono
    // passati, gli eventi dicono quando qualcosa e' cambiato.
    std::uint64_t node_events_recorded{};
    std::uint64_t source_events_recorded{};

    // Passi eseguiti senza che arrivasse un solo byte. Non sono passi sprecati:
    // sono quelli in cui il silenzio ha prodotto invecchiamento e, alla lunga,
    // la perdita del nodo.
    std::uint64_t silent_steps{};
};

// Porta i byte del controllore fino alla black box, passando per le protezioni.
//
// E' il secondo innesto mancante: la catena di ingestione esisteva e produceva
// uno stato protetto, il registratore a segmenti esisteva e sapeva scrivere, e
// nessuno dei due conosceva l'altro. `efis-recorder` registrava dal simulatore,
// quindi la black box conservava dati inventati mentre il Pixhawk parlava a
// vuoto.
//
// La sessione non decide da dove arrivano i byte: riceve una `ByteSource`, che
// puo' essere una registrazione da rileggere o, in futuro, un collegamento vivo.
// E' la separazione che rende il replay una prova vera e non una simulazione
// diversa: gli stessi byte producono lo stesso registro.
class RecordingSession final {
public:
    RecordingSession(recording::SegmentedRecorder& recorder,
                     PipelinePolicy policy = {});

    // Un passo: legge quanto disponibile, fa avanzare la catena e registra.
    //
    // Va chiamata anche quando la sorgente tace. Il tempo passa comunque, e con
    // esso scadenza, stasi e perdita del nodo: un passo silenzioso registra uno
    // stato che si sta degradando, ed e' esattamente cio' che serve ritrovare
    // rileggendo il volo.
    void step(io::ByteSource& source, MonoTime now);

    [[nodiscard]] const MavlinkPipeline& pipeline() const noexcept { return pipeline_; }
    [[nodiscard]] const RecordingSessionStatistics& statistics() const noexcept {
        return statistics_;
    }

private:
    void record_events();

    recording::SegmentedRecorder& recorder_;
    MavlinkPipeline pipeline_;
    std::vector<std::uint8_t> buffer_;
    RecordingSessionStatistics statistics_{};
};

}  // namespace efis::ingest
