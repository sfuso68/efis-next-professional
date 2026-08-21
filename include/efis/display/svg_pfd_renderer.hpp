#pragma once

#include "efis/display/display_model.hpp"

#include <string>

namespace efis::display {

struct RenderSize final {
    int width{1280};
    int height{800};
};

class SvgPfdRenderer final {
public:
    [[nodiscard]] std::string render(const PfdView& view, RenderSize size) const;
};

}  // namespace efis::display
