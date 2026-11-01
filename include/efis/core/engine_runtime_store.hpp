#pragma once
#include "efis/core/engine_runtime.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
namespace efis::engine {
struct RuntimeRecord { std::string profile_id; std::string engine_serial; RuntimeTotals totals; std::uint64_t revision{}; std::uint64_t saved_wall_ms{}; };

// Persistenza dei quattro contatori motore.
//
// Il file e' a righe `chiave=valore` con un checksum come ultima riga. Due
// proprieta' vanno conosciute da chi lo legge o lo scrive.
//
// `profile_id` ed `engine_serial` sono testo libero e vengono codificati in
// scrittura: senza codifica un ritorno a capo in uno dei due farebbe leggere le
// righe successive come campi, e poiche' precedono i totali un campo iniettato
// vincerebbe su quello reale. In lettura una chiave ripetuta, una riga senza
// separatore, una sequenza di escape sconosciuta e un totale negativo o non
// finito sono errori: `load` solleva un'eccezione e non restituisce un record
// parziale.
//
// Il checksum rileva la **corruzione accidentale** e non l'alterazione
// deliberata, perche' non vi entra alcun segreto: chi puo' scrivere il file puo'
// ricalcolarlo. `revision` non cambia per una modifica esterna, quindi nessun
// campo testimonia una riscrittura coerente. Il file non e' una prova di
// quante ore il motore abbia fatto.
class RuntimeStore {
public:
    explicit RuntimeStore(std::filesystem::path path) : path_(std::move(path)) {}
    [[nodiscard]] std::optional<RuntimeRecord> load() const;
    void save(const RuntimeRecord& record) const;
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};
}
