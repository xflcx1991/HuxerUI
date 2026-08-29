#pragma once

#include <string>
#include <string_view>

#include "parser.h"

namespace huxerui::declarative_codegen {

using huxerui::declarative::ParseError;

struct GeneratedSources {
  std::string header;
  std::string source;
};

[[nodiscard]] GeneratedSources GenerateSources(
    std::string_view input, std::string_view source_path, std::string_view namespace_name = "huxerui_generated"
);

} // namespace huxerui::declarative_codegen
