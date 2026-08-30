#pragma once

#include <filesystem>
#include <memory>

#include <huxerui/huxerui.h>

#include "../declarative_codegen/parser.h"

namespace huxerui::declarative_preview {

using ComponentHandle = std::shared_ptr<const huxerui::declarative::Component>;

struct PreviewSnapshot {
  std::string source;
  ComponentHandle component;
  std::string error;
  int line = 0;
  int column = 0;

  [[nodiscard]] bool IsValid() const noexcept {
    return component != nullptr;
  }
};

using PreviewSnapshotHandle = std::shared_ptr<const PreviewSnapshot>;

[[nodiscard]] PreviewSnapshotHandle LoadPreviewSnapshot(const std::filesystem::path& path);
[[nodiscard]] PreviewSnapshotHandle MakePreviewSnapshot(
    const std::filesystem::path& path, std::string source
);

[[nodiscard]] huxerui::View EvaluateComponent(ComponentHandle component);
[[nodiscard]] huxerui::View EvaluateComponent(const huxerui::declarative::Component& component);

[[nodiscard]] huxerui::View PreviewHost(const std::filesystem::path& path);

} // namespace huxerui::declarative_preview
