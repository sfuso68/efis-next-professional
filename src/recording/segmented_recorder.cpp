#include "efis/recording/segmented_recorder.hpp"

#include "efis/recording/flight_record.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace efis::recording {

SegmentedRecorder::SegmentedRecorder(std::filesystem::path directory,
                                     RecorderSettings settings)
    : directory_(std::move(directory)), settings_(settings) {
    if (settings_.records_per_segment == 0 || settings_.flush_every_records == 0) {
        throw std::invalid_argument("recorder settings must be positive");
    }
    std::filesystem::create_directories(directory_);
    recover_partials();
}

SegmentedRecorder::~SegmentedRecorder() {
    try { close(); } catch (...) { }
}

void SegmentedRecorder::recover_partials() {
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".partial") continue;
        if (entry.file_size() == 0) {
            std::filesystem::remove(entry.path());
            continue;
        }
        auto recovered = entry.path();
        recovered.replace_extension(".recovered.csv");
        std::filesystem::rename(entry.path(), recovered);
        ++recovered_segments_;
    }
}

void SegmentedRecorder::open_segment() {
    require_healthy();
    const auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    partial_path_ = directory_ /
        ("flight-" + std::to_string(wall_ms) + "-" +
         std::to_string(segment_index_++) + ".partial");
    stream_.open(partial_path_, std::ios::binary | std::ios::trunc);
    if (!stream_) fail("open segment");
    FlightRecordWriter::write_header(stream_);
    stream_.flush();
    verify_stream("write segment header");
    records_in_segment_ = 0;
    records_since_flush_ = 0;
}

void SegmentedRecorder::append(const FlightSnapshot& snapshot) {
    require_healthy();
    if (!stream_.is_open()) open_segment();
    FlightRecordWriter::write_snapshot(stream_, snapshot);
    const auto pending_since_flush = records_since_flush_ + 1;
    if (pending_since_flush >= settings_.flush_every_records) {
        stream_.flush();
        verify_stream("flush snapshot");
    }
    verify_stream("write snapshot");
    ++records_in_segment_;
    ++records_written_;
    records_since_flush_ = pending_since_flush >= settings_.flush_every_records
        ? 0 : pending_since_flush;
    if (records_in_segment_ >= settings_.records_per_segment) close_segment();
}

void SegmentedRecorder::append(const NodeStateChange& event) {
    require_healthy();
    if (!stream_.is_open()) open_segment();
    FlightRecordWriter::write_node_event(stream_, event);
    const auto pending_since_flush = records_since_flush_ + 1;
    if (pending_since_flush >= settings_.flush_every_records) {
        stream_.flush();
        verify_stream("flush node event");
    }
    verify_stream("write node event");
    records_since_flush_ = pending_since_flush >= settings_.flush_every_records
        ? 0 : pending_since_flush;
}

void SegmentedRecorder::append(const SourceChangeEvent& event) {
    require_healthy();
    if (!stream_.is_open()) open_segment();
    FlightRecordWriter::write_source_change(stream_, event);
    const auto pending_since_flush = records_since_flush_ + 1;
    if (pending_since_flush >= settings_.flush_every_records) {
        stream_.flush();
        verify_stream("flush source event");
    }
    verify_stream("write source event");
    records_since_flush_ = pending_since_flush >= settings_.flush_every_records
        ? 0 : pending_since_flush;
}

void SegmentedRecorder::close_segment() {
    if (!stream_.is_open()) return;
    if (write_failed_) {
        stream_.close();
        throw std::runtime_error{"recorder storage failure: " + last_error_};
    }
    stream_.flush();
    verify_stream("flush segment before close");
    stream_.close();
    verify_stream("close segment");
    auto final_path = partial_path_;
    final_path.replace_extension(".csv");
    std::error_code error;
    std::filesystem::rename(partial_path_, final_path, error);
    if (error) fail("publish completed segment: " + error.message());
    partial_path_.clear();
}

void SegmentedRecorder::close() { close_segment(); }

void SegmentedRecorder::require_healthy() const {
    if (write_failed_) {
        throw std::runtime_error{"recorder storage failure: " + last_error_};
    }
}

void SegmentedRecorder::verify_stream(const char* operation) {
    if (!stream_.good()) fail(operation);
}

[[noreturn]] void SegmentedRecorder::fail(const std::string& operation) {
    write_failed_ = true;
    last_error_ = operation;
    throw std::runtime_error{"recorder storage failure: " + operation};
}

}  // namespace efis::recording
