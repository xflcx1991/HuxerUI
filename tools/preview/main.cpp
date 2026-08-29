#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <huxerui/app.h>
#include <huxerui/huxerui.h>

#include "evaluator.h"

namespace {

huxerui::View g_root_view;
// The generated View tree references the parsed Document through deferred
// Scope factories, so both outlive RunApplication.
std::string g_ui_content;
huxerui::declarative::Document g_ui_document;

huxerui::View PreviewRoot() {
  return std::move(g_root_view);
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("unable to open input file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

} // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "usage: huxerui_preview <file.ui>\n";
    return 1;
  }

  const std::filesystem::path ui_path = argv[1];
  try {
    g_ui_content = ReadFile(ui_path);
    g_ui_document = huxerui::declarative::ParseDocument(g_ui_content, ui_path.string());
    if (g_ui_document.components.size() != 1) {
      std::cerr << "huxerui_preview: expected exactly one component, got "
                << g_ui_document.components.size() << "\n";
      return 1;
    }
    g_root_view = huxerui::declarative_preview::EvaluateComponent(g_ui_document.components.front());
  } catch (const huxerui::declarative::ParseError& error) {
    std::cerr << error.what() << ':' << error.Line() << ':' << error.Column() << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "huxerui_preview: error: " << error.what() << "\n";
    return 1;
  }

  static const huxerui::Application application{
      PreviewRoot,
      {
          .window = {
              .title = "HuxerUI Preview",
              .initial_size = {520.0F, 360.0F},
          },
      }
  };
  const int exit_code = huxerui::RunApplication();
  return exit_code;
}
