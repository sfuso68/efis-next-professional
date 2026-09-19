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
};

void write_recorder_status(const std::filesystem::path& path,
                           const RecorderStatus& status);
[[nodiscard]] RecorderStatus read_recorder_status(const std::filesystem::path& path);

}  // namespace efis::recording
