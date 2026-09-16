#pragma once
#include "efis/core/engine_runtime.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
namespace efis::engine {
struct RuntimeRecord { std::string profile_id; std::string engine_serial; RuntimeTotals totals; std::uint64_t revision{}; std::uint64_t saved_wall_ms{}; };
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
