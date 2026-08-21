#include "efis/ingest/recording_session.hpp"

namespace efis::ingest {
namespace {

// Un blocco di lettura piu' grande del payload massimo di MAVLink, cosi' un
// frame intero entra sempre in un solo passo. Il parser regge comunque i frame
// spezzati, ma leggerne uno per volta renderebbe il replay inutilmente lento.
constexpr std::size_t read_chunk = 4096;

}  // namespace

RecordingSession::RecordingSession(recording::SegmentedRecorder& recorder,
                                   PipelinePolicy policy)
    : recorder_(recorder), pipeline_(policy), buffer_(read_chunk) {}

void RecordingSession::step(io::ByteSource& source, MonoTime now) {
    const std::size_t read = source.read(buffer_);
    statistics_.bytes_read += read;
    if (read == 0) {
        ++statistics_.silent_steps;
    } else {
        pipeline_.consume(std::span<const std::uint8_t>{buffer_.data(), read}, now);
    }

    // Anche dopo aver consumato dei byte: consume() non applica i timeout dei
    // nodi, e il tempo scorre allo stesso modo per un collegamento parlante e
    // per uno muto.
    pipeline_.update(now);

    // Gli eventi vanno scritti prima dello stato che li ha prodotti. Rileggendo,
    // la perdita di un nodo deve comparire prima dei campioni che ne portano le
    // conseguenze, altrimenti il registro racconta l'effetto prima della causa.
    record_events();

    recorder_.append(pipeline_.state());
    ++statistics_.snapshots_recorded;
}

void RecordingSession::record_events() {
    for (const NodeStateChange& event : pipeline_.node_events()) {
        recorder_.append(event);
        ++statistics_.node_events_recorded;
    }
    for (const SourceChangeEvent& event : pipeline_.source_events()) {
        recorder_.append(event);
        ++statistics_.source_events_recorded;
    }
    // Svuotare e' parte della scrittura, non una pulizia opzionale: un evento
    // lasciato in coda verrebbe registrato di nuovo al passo successivo, e una
    // registrazione che ripete la stessa perdita di nodo cento volte non e' piu'
    // leggibile.
    pipeline_.clear_events();
}

}  // namespace efis::ingest
