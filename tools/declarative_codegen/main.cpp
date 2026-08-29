#include "generator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Arguments {
  std::filesystem::path input;
  std::filesystem::path output_cpp;
  std::filesystem::path output_header;
  std::string namespace_name = "huxerui_generated";
};

[[nodiscard]] Arguments ParseArguments(int argc, char** argv) {
  Arguments result;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--input" && index + 1 < argc) {
      result.input = argv[++index];
    } else if (argument == "--output-cpp" && index + 1 < argc) {
      result.output_cpp = argv[++index];
    } else if (argument == "--output-header" && index + 1 < argc) {
      result.output_header = argv[++index];
    } else if (argument == "--namespace" && index + 1 < argc) {
      result.namespace_name = argv[++index];
    } else {
      throw std::invalid_argument(
          "usage: huxerui_declarative_codegen --input <path> --output-cpp <path> --output-header <path> "
          "[--namespace <name>]"
      );
    }
  }
  if (result.input.empty() || result.output_cpp.empty() || result.output_header.empty()) {
    throw std::invalid_argument(
        "usage: huxerui_declarative_codegen --input <path> --output-cpp <path> --output-header <path> "
        "[--namespace <name>]"
    );
  }
  return result;
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("unable to open input file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void WriteFile(const std::filesystem::path& path, std::string_view content) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("unable to open output file: " + path.string());
  }
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!stream) {
    throw std::runtime_error("unable to write output file: " + path.string());
  }
}

[[nodiscard]] std::string GeneratedHeaderStem(const std::filesystem::path& path) {
  const std::string filename = path.filename().string();
  constexpr std::string_view suffix = ".generated.h";
  if (filename.size() >= suffix.size() && filename.ends_with(suffix)) {
    return filename.substr(0, filename.size() - suffix.size());
  }
  return path.stem().string();
}

} // namespace

int main(int argc, char** argv) {
  try {
    const Arguments arguments = ParseArguments(argc, argv);
    const auto generated = huxerui::declarative_codegen::GenerateSources(
        ReadFile(arguments.input),
        GeneratedHeaderStem(arguments.output_header),
        arguments.namespace_name
    );
    WriteFile(arguments.output_header, generated.header);
    WriteFile(arguments.output_cpp, generated.source);
  } catch (const huxerui::declarative::ParseError& error) {
    std::cerr << error.what() << ':' << error.Line() << ':' << error.Column() << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "huxerui_declarative_codegen: error: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
