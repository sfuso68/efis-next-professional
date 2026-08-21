#include "efis/io/byte_source.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace efis::io {
namespace {

std::vector<std::uint8_t> load(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) return {};
    return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{file},
                                     std::istreambuf_iterator<char>{}};
}

}  // namespace

RecordedByteSource::RecordedByteSource(const std::filesystem::path& path,
                                       std::size_t chunk)
    : bytes_(load(path)), chunk_(chunk == 0 ? 1 : chunk) {}

RecordedByteSource::RecordedByteSource(std::vector<std::uint8_t> bytes,
                                       std::size_t chunk)
    : bytes_(std::move(bytes)), chunk_(chunk == 0 ? 1 : chunk) {}

std::size_t RecordedByteSource::read(std::span<std::uint8_t> buffer) {
    const std::size_t remaining = bytes_.size() - position_;
    const std::size_t count = std::min({remaining, chunk_, buffer.size()});
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), count,
                buffer.begin());
    position_ += count;
    return count;
}

bool RecordedByteSource::exhausted() const noexcept {
    return position_ >= bytes_.size();
}

}  // namespace efis::io
