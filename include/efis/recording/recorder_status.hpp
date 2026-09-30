#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace efis::recording {

struct RecorderStatus final {
    std::string state{"OFFLINE"};
    std::uint64_t updated_wall_ms{};
    std::uint64_t records{};
    std::string segment;
    bool write_failed{};
    std::string last_error;
    bool retention_enabled{};
    std::uint64_t completed_segments{};
    std::uint64_t completed_bytes{};
    std::uint64_t available_bytes{};
    std::uint64_t pruned_segments{};
    std::uint64_t pruned_bytes{};
};

void write_recorder_status(const std::filesystem::path& path,
                           const RecorderStatus& status);
[[nodiscard]] RecorderStatus read_recorder_status(const std::filesystem::path& path);

}  // namespace efis::recording
