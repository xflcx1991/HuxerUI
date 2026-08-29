#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace huxerui::declarative {

struct Property {
  std::string name;
  std::string expression;
  std::size_t offset = 0;
  int line = 1;
  int column = 1;
};

struct Node {
  std::string type;
  std::vector<Property> properties;
  std::vector<Node> children;
  std::size_t offset = 0;
  int line = 1;
  int column = 1;
};

struct Component {
  std::string name;
  std::vector<Node> children;
  std::size_t offset = 0;
  int line = 1;
  int column = 1;

  [[nodiscard]] const Node* FindRoot() const noexcept;
};

struct Document {
  std::vector<Component> components;
};

class ParseError final : public std::runtime_error {
public:
  ParseError(std::size_t offset, int line, int column, std::string message);

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

class ExpressionParser final {
public:
  ExpressionParser(
      std::string_view expression,
      const std::map<std::string, std::string>& states,
      std::size_t offset = 0,
      int line = 1,
      int column = 1
  );

  [[nodiscard]] std::string Parse();

private:
  [[nodiscard]] std::string ParseBinary(int minimum_precedence);
  [[nodiscard]] std::string ParseUnary();
  void Expect(std::string_view text);
  void Advance();
  [[nodiscard]] static int Precedence(std::string_view operation);
  [[noreturn]] void Error(std::string message) const;

  std::string_view expression_;
  const std::map<std::string, std::string>& states_;
  std::size_t offset_;
  int line_;
  int column_;
  std::size_t position_ = 0;
  struct Token {
    int kind = 0;
    std::string text;
  };
  Token current_;
};

[[nodiscard]] Document ParseDocument(std::string_view input, std::string_view source_path);

[[nodiscard]] const Property* FindProperty(const Node& node, std::string_view name);

[[noreturn]] void ThrowParseError(
    std::size_t offset, int line, int column, std::string message
);

[[nodiscard]] std::string ParseStringLiteral(const Property& property);

[[nodiscard]] std::string Trim(std::string value);

} // namespace huxerui::declarative
