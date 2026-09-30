#include "efis/recording/recorder_status.hpp"

#include <fstream>
#include <stdexcept>

namespace efis::recording {

void write_recorder_status(const std::filesystem::path& path,
                           const RecorderStatus& status) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".partial";
    {
        std::ofstream output{temporary, std::ios::trunc};
        if (!output) throw std::runtime_error("unable to write recorder status");
        output << "EFIS_RECORDER_STATUS_V3\n"
               << "state=" << status.state << '\n'
               << "updated_wall_ms=" << status.updated_wall_ms << '\n'
               << "records=" << status.records << '\n'
               << "segment=" << status.segment << '\n'
               << "write_failed=" << (status.write_failed ? 1 : 0) << '\n'
               << "last_error=" << status.last_error << '\n'
               << "retention_enabled=" << (status.retention_enabled ? 1 : 0) << '\n'
               << "completed_segments=" << status.completed_segments << '\n'
               << "completed_bytes=" << status.completed_bytes << '\n'
               << "available_bytes=" << status.available_bytes << '\n'
               << "pruned_segments=" << status.pruned_segments << '\n'
               << "pruned_bytes=" << status.pruned_bytes << '\n';
        output.flush();
        if (!output) throw std::runtime_error("unable to flush recorder status");
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
    if (error) throw std::runtime_error("unable to publish recorder status");
}

RecorderStatus read_recorder_status(const std::filesystem::path& path) {
    std::ifstream input{path};
    std::string line;
    if (!input || !std::getline(input, line) ||
        (line != "EFIS_RECORDER_STATUS_V1" && line != "EFIS_RECORDER_STATUS_V2" &&
         line != "EFIS_RECORDER_STATUS_V3")) {
        throw std::runtime_error("invalid recorder status");
    }
    RecorderStatus status;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const auto key = line.substr(0, separator);
        const auto value = line.substr(separator + 1);
        if (key == "state") status.state = value;
        else if (key == "updated_wall_ms") status.updated_wall_ms = std::stoull(value);
        else if (key == "records") status.records = std::stoull(value);
        else if (key == "segment") status.segment = value;
        else if (key == "write_failed") status.write_failed = value == "1";
        else if (key == "last_error") status.last_error = value;
        else if (key == "retention_enabled") status.retention_enabled = value == "1";
        else if (key == "completed_segments") status.completed_segments = std::stoull(value);
        else if (key == "completed_bytes") status.completed_bytes = std::stoull(value);
        else if (key == "available_bytes") status.available_bytes = std::stoull(value);
        else if (key == "pruned_segments") status.pruned_segments = std::stoull(value);
        else if (key == "pruned_bytes") status.pruned_bytes = std::stoull(value);
    }
    return status;
}

}  // namespace efis::recording
