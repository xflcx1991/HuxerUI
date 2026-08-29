#pragma once

#include <huxerui/huxerui.h>

#include "../declarative_codegen/parser.h"

namespace huxerui::declarative_preview {

[[nodiscard]] huxerui::View EvaluateComponent(
    const huxerui::declarative::Component& component
);

} // namespace huxerui::declarative_preview
