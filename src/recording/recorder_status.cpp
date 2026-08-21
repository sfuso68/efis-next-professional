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
        output << "EFIS_RECORDER_STATUS_V1\n"
               << "state=" << status.state << '\n'
               << "updated_wall_ms=" << status.updated_wall_ms << '\n'
               << "records=" << status.records << '\n'
               << "segment=" << status.segment << '\n';
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
    if (!input || !std::getline(input, line) || line != "EFIS_RECORDER_STATUS_V1") {
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
    }
    return status;
}

}  // namespace efis::recording
