#include "efis/recording/segmented_recorder.hpp"

#include "efis/recording/flight_record.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace efis::recording {

SegmentedRecorder::SegmentedRecorder(std::filesystem::path directory,
                                     RecorderSettings settings)
    : directory_(std::move(directory)), settings_(settings) {
    if (settings_.records_per_segment == 0 || settings_.flush_every_records == 0) {
        throw std::invalid_argument("recorder settings must be positive");
    }
    std::filesystem::create_directories(directory_);
    recover_partials();
    enforce_retention();
}

void SegmentedRecorder::enforce_retention() {
    struct CompletedSegment final {
        std::filesystem::path path;
        std::filesystem::file_time_type modified;
        std::uintmax_t bytes{};
    };

    std::vector<CompletedSegment> segments;
    std::uintmax_t total_bytes{};
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (entry.is_symlink() || !entry.is_regular_file()) continue;
        const auto name = entry.path().filename().string();
        if (!name.starts_with("flight-") || !name.ends_with(".csv")) continue;
        const auto bytes = entry.file_size(error);
        if (error) fail("inspect completed segment size: " + error.message());
        const auto modified = entry.last_write_time(error);
        if (error) fail("inspect completed segment timestamp: " + error.message());
        if (total_bytes > std::numeric_limits<std::uintmax_t>::max() - bytes) {
            fail("completed segment byte total overflow");
        }
        segments.push_back({entry.path(), modified, bytes});
        total_bytes += bytes;
    }
    std::ranges::sort(segments, [](const auto& left, const auto& right) {
        if (left.modified != right.modified) return left.modified < right.modified;
        return left.path.filename() < right.path.filename();
    });

    const auto space = std::filesystem::space(directory_, error);
    if (error) fail("inspect recorder filesystem: " + error.message());
    completed_segments_ = segments.size();
    completed_bytes_ = total_bytes;
    available_bytes_ = space.available;
    if (!retention_enabled()) return;

    const auto bytes_over_quota = settings_.max_completed_bytes != 0 &&
            total_bytes > settings_.max_completed_bytes
        ? total_bytes - settings_.max_completed_bytes : 0;
    const auto bytes_below_reserve = settings_.min_free_bytes != 0 &&
            space.available < settings_.min_free_bytes
        ? settings_.min_free_bytes - space.available : 0;
    const auto required = std::max(bytes_over_quota, bytes_below_reserve);
    if (required == 0) return;

    const auto removable_count = segments.size() > settings_.minimum_completed_segments
        ? segments.size() - settings_.minimum_completed_segments : 0;
    std::uintmax_t reclaimable{};
    for (std::size_t index = 0; index < removable_count; ++index) {
        if (reclaimable > std::numeric_limits<std::uintmax_t>::max() -
                              segments[index].bytes) {
            fail("retention reclaimable byte total overflow");
        }
        reclaimable += segments[index].bytes;
        if (reclaimable >= required) break;
    }
    if (reclaimable < required) {
        fail("retention target cannot be met without deleting protected recent segments");
    }

    std::uintmax_t reclaimed{};
    std::size_t removed{};
    while (removed < removable_count && reclaimed < required) {
        std::filesystem::remove(segments[removed].path, error);
        if (error) fail("remove expired completed segment: " + error.message());
        reclaimed += segments[removed].bytes;
        ++removed;
    }
    pruned_segments_ += removed;
    pruned_bytes_ += reclaimed;
    completed_segments_ -= removed;
    completed_bytes_ -= reclaimed;
    const auto updated_space = std::filesystem::space(directory_, error);
    if (error) fail("inspect recorder filesystem after retention: " + error.message());
    available_bytes_ = updated_space.available;
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
    enforce_retention();
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
