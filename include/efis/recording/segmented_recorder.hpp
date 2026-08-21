#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace efis::recording {

struct RecorderSettings final {
    std::size_t records_per_segment{3000};
    std::size_t flush_every_records{50};
};

class SegmentedRecorder final {
public:
    explicit SegmentedRecorder(std::filesystem::path directory,
                               RecorderSettings settings = {});
    ~SegmentedRecorder();

    SegmentedRecorder(const SegmentedRecorder&) = delete;
    SegmentedRecorder& operator=(const SegmentedRecorder&) = delete;

    void append(const FlightSnapshot& snapshot);
    void close();

    [[nodiscard]] std::size_t recovered_segments() const noexcept {
        return recovered_segments_;
    }
    [[nodiscard]] std::string current_segment_name() const {
        return partial_path_.filename().string();
    }

private:
    void recover_partials();
    void open_segment();
    void close_segment();

    std::filesystem::path directory_;
    RecorderSettings settings_;
    std::ofstream stream_;
    std::filesystem::path partial_path_;
    std::size_t segment_index_{};
    std::size_t records_in_segment_{};
    std::size_t records_since_flush_{};
    std::size_t recovered_segments_{};
};

}  // namespace efis::recording
