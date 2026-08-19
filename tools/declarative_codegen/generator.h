#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace huxerui::declarative_codegen {

class GeneratorError final : public std::runtime_error {
public:
  GeneratorError(std::size_t offset, int line, int column, std::string message);

  [[nodiscard]] std::size_t Offset() const noexcept {
    return offset_;
  }

  [[nodiscard]] int Line() const noexcept {
    return line_;
  }

  [[nodiscard]] int Column() const noexcept {
    return column_;
  }

private:
  std::size_t offset_;
  int line_;
  int column_;
};

struct GeneratedSources {
  std::string header;
  std::string source;
};

[[nodiscard]] GeneratedSources GenerateSources(
    std::string_view input, std::string_view source_path, std::string_view namespace_name = "huxerui_generated"
);

} // namespace huxerui::declarative_codegen
