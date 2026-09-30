#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/node_health.hpp"
#include "efis/core/source_arbitrator.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace efis::recording {

struct RecorderSettings final {
    std::size_t records_per_segment{3000};
    std::size_t flush_every_records{50};
    // Zero disables the corresponding retention constraint. Retention only
    // ever removes completed, recorder-owned flight-*.csv segments.
    std::uintmax_t max_completed_bytes{};
    std::uintmax_t min_free_bytes{};
    std::size_t minimum_completed_segments{1};
};

class SegmentedRecorder final {
public:
    explicit SegmentedRecorder(std::filesystem::path directory,
                               RecorderSettings settings = {});
    ~SegmentedRecorder();

    SegmentedRecorder(const SegmentedRecorder&) = delete;
    SegmentedRecorder& operator=(const SegmentedRecorder&) = delete;

    void append(const FlightSnapshot& snapshot);

    // Eventi, accanto ai valori.
    //
    // Il paragrafo 17.1 dell'handoff chiede un registro degli eventi separato da
    // quello dei campioni, e il formato V2 li prevede da sempre: mancava chi li
    // scrivesse. Senza, una registrazione dice quali valori sono passati ma non
    // quando un nodo se n'e' andato ne' quando una grandezza ha cambiato
    // origine, che sono le due domande a cui si vuole rispondere rileggendo un
    // volo andato storto.
    //
    // Non contano verso la rotazione del segmento, che misura i campioni: un
    // evento e' raro e non deve spezzare un segmento a meta'.
    void append(const NodeStateChange& event);
    void append(const SourceChangeEvent& event);

    void close();

    [[nodiscard]] std::size_t recovered_segments() const noexcept {
        return recovered_segments_;
    }
    [[nodiscard]] std::string current_segment_name() const {
        return partial_path_.filename().string();
    }
    [[nodiscard]] bool healthy() const noexcept { return !write_failed_; }
    [[nodiscard]] std::size_t records_written() const noexcept {
        return records_written_;
    }
    [[nodiscard]] const std::string& last_error() const noexcept {
        return last_error_;
    }
    [[nodiscard]] bool retention_enabled() const noexcept {
        return settings_.max_completed_bytes != 0 || settings_.min_free_bytes != 0;
    }
    [[nodiscard]] std::size_t completed_segments() const noexcept {
        return completed_segments_;
    }
    [[nodiscard]] std::uintmax_t completed_bytes() const noexcept {
        return completed_bytes_;
    }
    [[nodiscard]] std::uintmax_t available_bytes() const noexcept {
        return available_bytes_;
    }
    [[nodiscard]] std::size_t pruned_segments() const noexcept {
        return pruned_segments_;
    }
    [[nodiscard]] std::uintmax_t pruned_bytes() const noexcept {
        return pruned_bytes_;
    }

private:
    friend class SegmentedRecorderTestAccess;

    void recover_partials();
    void enforce_retention();
    void open_segment();
    void close_segment();
    void require_healthy() const;
    void verify_stream(const char* operation);
    [[noreturn]] void fail(const std::string& operation);

    std::filesystem::path directory_;
    RecorderSettings settings_;
    std::ofstream stream_;
    std::filesystem::path partial_path_;
    std::size_t segment_index_{};
    std::size_t records_in_segment_{};
    std::size_t records_since_flush_{};
    std::size_t recovered_segments_{};
    std::size_t records_written_{};
    std::size_t completed_segments_{};
    std::uintmax_t completed_bytes_{};
    std::uintmax_t available_bytes_{};
    std::size_t pruned_segments_{};
    std::uintmax_t pruned_bytes_{};
    bool write_failed_{};
    std::string last_error_;
};

}  // namespace efis::recording
