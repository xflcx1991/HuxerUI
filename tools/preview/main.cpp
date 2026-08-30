#include <filesystem>
#include <iostream>
#include <string>

#include <huxerui/app.h>
#include <huxerui/huxerui.h>

#include "evaluator.h"

namespace {

std::filesystem::path g_ui_path;

huxerui::View PreviewRoot() {
  return huxerui::declarative_preview::PreviewHost(g_ui_path);
}

} // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: huxerui_preview <file.ui>\n";
    return 1;
  }

  g_ui_path = std::filesystem::absolute(argv[1]);

  static const huxerui::Application application{
      PreviewRoot,
      {
          .window = {
              .title = "HuxerUI Preview",
              .initial_size = {520.0F, 360.0F},
          },
      }
  };
  return huxerui::RunApplication();
}
