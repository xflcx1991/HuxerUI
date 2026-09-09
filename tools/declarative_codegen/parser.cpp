#include "parser.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace huxerui::declarative {

namespace {

enum class TokenKind {
  End,
  Identifier,
  Number,
  Operator,
};

constexpr int kTokenEnd = 0;
constexpr int kTokenIdentifier = 1;
constexpr int kTokenNumber = 2;
constexpr int kTokenOperator = 3;

std::string TrimValue(std::string value) {
  const auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char character) {
                return !is_space(static_cast<unsigned char>(character));
              }));
  value.erase(
      std::find_if(
          value.rbegin(),
          value.rend(),
          [&](char character) { return !is_space(static_cast<unsigned char>(character)); }
      ).base(),
      value.end()
  );
  return value;
}

class DocumentParser final {
public:
  DocumentParser(std::string_view source, std::string_view source_path)
      : source_(source), source_path_(source_path) {}

  [[nodiscard]] Document Parse() {
    Document document;
    SkipTrivia();
    document.components.push_back(ParseComponent());
    SkipTrivia();
    if (!End()) {
      Fail("a declarative file must contain exactly one component declaration");
    }
    return document;
  }

private:
  [[nodiscard]] Component ParseComponent() {
    const std::size_t offset = position_;
    const int line = line_;
    const int column = column_;
    Component component;
    component.name = ParseIdentifier();
    component.offset = offset;
    component.line = line;
    component.column = column;
    ParseBlock(component.children);
    return component;
  }

  void ParseBlock(std::vector<Node>& children) {
    SkipTrivia();
    Consume('{');
    SkipTrivia();
    while (!End() && Peek() != '}') {
      const std::size_t offset = position_;
      const int line = line_;
      const int column = column_;
      const std::string name = ParseIdentifier();
      SkipInlineSpace();
      if (ConsumeIf(':')) {
        Fail("properties are only valid inside a node or state block", offset, line, column);
      }
      if (Peek() != '{') {
        Fail("expected '{' after node name", offset, line, column);
      }
      Node node;
      node.type = name;
      node.offset = offset;
      node.line = line;
      node.column = column;
      ParseNodeBlock(node);
      children.push_back(std::move(node));
      SkipTrivia();
    }
    Consume('}');
  }

  void ParseNodeBlock(Node& node) {
    Consume('{');
    SkipTrivia();
    while (!End() && Peek() != '}') {
      const std::size_t offset = position_;
      const int line = line_;
      const int column = column_;
      const std::string name = ParseIdentifier();
      SkipInlineSpace();
      if (ConsumeIf(':')) {
        Property property;
        property.name = name;
        property.expression = ParseExpression();
        property.offset = offset;
        property.line = line;
        property.column = column;
        if (property.expression.empty()) {
          Fail("expected an expression after ':'", offset, line, column);
        }
        if (std::any_of(node.properties.begin(), node.properties.end(), [&](const Property& existing) {
              return existing.name == property.name;
            })) {
          Fail("property '" + property.name + "' is declared more than once", offset, line, column);
        }
        node.properties.push_back(std::move(property));
        ConsumeIf(';');
      } else if (Peek() == '{') {
        Node child;
        child.type = name;
        child.offset = offset;
        child.line = line;
        child.column = column;
        ParseNodeBlock(child);
        node.children.push_back(std::move(child));
      } else {
        Fail("expected ':' or '{' after '" + name + "'", offset, line, column);
      }
      SkipTrivia();
    }
    Consume('}');
  }

  [[nodiscard]] std::string ParseExpression() {
    SkipInlineSpace();
    const std::size_t start = position_;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    char quote = '\0';
    while (!End()) {
      const char character = Peek();
      if (quote != '\0') {
        Advance();
        if (character == '\\' && !End()) {
          Advance();
        } else if (character == quote) {
          quote = '\0';
        }
        continue;
      }
      if (character == '\'' || character == '"') {
        quote = character;
        Advance();
        continue;
      }
      if (character == '(') {
        ++parentheses;
      } else if (character == ')') {
        --parentheses;
      } else if (character == '[') {
        ++brackets;
      } else if (character == ']') {
        --brackets;
      } else if (character == '{') {
        ++braces;
      } else if (character == '}') {
        if (parentheses == 0 && brackets == 0 && braces == 0) {
          break;
        }
        --braces;
      }
      if ((character == '\n' || character == ';') && parentheses == 0 && brackets == 0 && braces == 0) {
        break;
      }
      Advance();
    }
    if (quote != '\0') {
      Fail("unterminated string literal", start, line_, column_);
    }
    return TrimValue(std::string(source_.substr(start, position_ - start)));
  }

  [[nodiscard]] std::string ParseIdentifier() {
    if (End() || (!std::isalpha(static_cast<unsigned char>(Peek())) && Peek() != '_')) {
      Fail("expected identifier");
    }
    const std::size_t start = position_;
    Advance();
    while (!End() && (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_')) {
      Advance();
    }
    return std::string(source_.substr(start, position_ - start));
  }

  void SkipTrivia() {
    while (!End()) {
      if (std::isspace(static_cast<unsigned char>(Peek())) != 0) {
        Advance();
        continue;
      }
      if (Peek() == '/' && Next() == '/') {
        Advance();
        Advance();
        while (!End() && Peek() != '\n') {
          Advance();
        }
        continue;
      }
      return;
    }
  }

  void SkipInlineSpace() {
    while (!End() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\r')) {
      Advance();
    }
  }

  void Consume(char expected) {
    if (!ConsumeIf(expected)) {
      Fail(std::string("expected '") + expected + "'");
    }
  }

  bool ConsumeIf(char expected) {
    if (!End() && Peek() == expected) {
      Advance();
      return true;
    }
    return false;
  }

  [[nodiscard]] char Peek() const {
    return End() ? '\0' : source_[position_];
  }

  [[nodiscard]] char Next() const {
    return position_ + 1 < source_.size() ? source_[position_ + 1] : '\0';
  }

  [[nodiscard]] bool End() const {
    return position_ >= source_.size();
  }

  void Advance() {
    if (End()) {
      return;
    }
    if (source_[position_++] == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
  }

  [[noreturn]] void Fail(std::string message) const {
    Fail(std::move(message), position_, line_, column_);
  }

  [[noreturn]] void Fail(std::string message, std::size_t offset, int line, int column) const {
    std::string full_message = source_path_.empty() ? std::move(message) : std::string(source_path_) + ": " + std::move(message);
    throw ParseError(offset, line, column, std::move(full_message));
  }

  std::string_view source_;
  std::string_view source_path_;
  std::size_t position_ = 0;
  int line_ = 1;
  int column_ = 1;
};

} // namespace

// ExpressionParser implementation

ExpressionParser::ExpressionParser(
    std::string_view expression,
    const std::map<std::string, std::string>& states,
    std::size_t offset,
    int line,
    int column
)
    : expression_(expression), states_(states), offset_(offset), line_(line), column_(column) {
  Advance();
}

std::string ExpressionParser::Parse() {
  const std::string result = ParseBinary(0);
  if (current_.kind != static_cast<int>(TokenKind::End)) {
    Error("unexpected token '" + current_.text + "'");
  }
  return result;
}

std::string ExpressionParser::ParseBinary(int minimum_precedence) {
  std::string left = ParseUnary();
  while (current_.kind == static_cast<int>(TokenKind::Operator)) {
    const int precedence = Precedence(current_.text);
    if (precedence < minimum_precedence) {
      break;
    }
    const std::string operation = current_.text;
    Advance();
    const std::string right = ParseBinary(precedence + 1);
    left = "(" + left + " " + operation + " " + right + ")";
  }
  return left;
}

std::string ExpressionParser::ParseUnary() {
  if (current_.kind == static_cast<int>(TokenKind::Operator) &&
      (current_.text == "!" || current_.text == "-")) {
    const std::string operation = current_.text;
    Advance();
    return "(" + operation + ParseUnary() + ")";
  }
  if (current_.kind == static_cast<int>(TokenKind::Operator) && current_.text == "(") {
    Advance();
    const std::string result = ParseBinary(0);
    Expect(")");
    return "(" + result + ")";
  }
  if (current_.kind == static_cast<int>(TokenKind::Identifier)) {
    const std::string identifier = current_.text;
    Advance();
    if (identifier != "true" && identifier != "false" && states_.find(identifier) == states_.end()) {
      Error("unknown state '" + identifier + "'");
    }
    return identifier;
  }
  if (current_.kind == static_cast<int>(TokenKind::Number)) {
    const std::string number = current_.text;
    Advance();
    return number;
  }
  Error("expected a literal, state, or parenthesized expression");
}

void ExpressionParser::Expect(std::string_view text) {
  if (current_.kind != static_cast<int>(TokenKind::Operator) || current_.text != text) {
    Error("expected '" + std::string(text) + "'");
  }
  Advance();
}

void ExpressionParser::Advance() {
  while (position_ < expression_.size() && std::isspace(static_cast<unsigned char>(expression_[position_])) != 0) {
    ++position_;
  }
  if (position_ >= expression_.size()) {
    current_ = {};
    return;
  }
  const char character = expression_[position_];
  if (std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_') {
    const std::size_t start = position_++;
    while (position_ < expression_.size() &&
           (std::isalnum(static_cast<unsigned char>(expression_[position_])) || expression_[position_] == '_')) {
      ++position_;
    }
    current_ = {static_cast<int>(TokenKind::Identifier), std::string(expression_.substr(start, position_ - start))};
    return;
  }
  if (std::isdigit(static_cast<unsigned char>(character)) != 0 || character == '.') {
    const std::size_t start = position_++;
    while (position_ < expression_.size() &&
           (std::isdigit(static_cast<unsigned char>(expression_[position_])) || expression_[position_] == '.')) {
      ++position_;
    }
    current_ = {static_cast<int>(TokenKind::Number), std::string(expression_.substr(start, position_ - start))};
    return;
  }
  const std::string two_character =
      position_ + 1 < expression_.size() ? std::string(expression_.substr(position_, 2)) : std::string{};
  if (two_character == "<=" || two_character == ">=" || two_character == "==" || two_character == "!=" ||
      two_character == "&&" || two_character == "||") {
    position_ += 2;
    current_ = {static_cast<int>(TokenKind::Operator), two_character};
    return;
  }
  if (std::string_view("+-*/!<>()").find(character) != std::string_view::npos) {
    ++position_;
    current_ = {static_cast<int>(TokenKind::Operator), std::string(1, character)};
    return;
  }
  Error("unsupported expression character");
}

int ExpressionParser::Precedence(std::string_view operation) {
  if (operation == "||")
    return 1;
  if (operation == "&&")
    return 2;
  if (operation == "==" || operation == "!=")
    return 3;
  if (operation == "<" || operation == "<=" || operation == ">" || operation == ">=")
    return 4;
  if (operation == "+" || operation == "-")
    return 5;
  if (operation == "*" || operation == "/")
    return 6;
  return -1;
}

[[noreturn]] void ExpressionParser::Error(std::string message) const {
  ThrowParseError(offset_, line_, column_, "expression: " + std::move(message));
}

// Public API

ParseError::ParseError(std::size_t offset, int line, int column, std::string message)
    : std::runtime_error(std::move(message)), offset_(offset), line_(line), column_(column) {}

Document ParseDocument(std::string_view input, std::string_view source_path) {
  return DocumentParser(input, source_path).Parse();
}

const Property* FindProperty(const Node& node, std::string_view name) {
  const auto found = std::find_if(node.properties.begin(), node.properties.end(), [&](const Property& property) {
    return property.name == name;
  });
  return found == node.properties.end() ? nullptr : &*found;
}

void ThrowParseError(std::size_t offset, int line, int column, std::string message) {
  throw ParseError(offset, line, column, std::move(message));
}

std::string Trim(std::string value) {
  return TrimValue(std::move(value));
}

const Node* Component::FindRoot() const noexcept {
  const Node* root = nullptr;
  for (const Node& child : children) {
    if (child.type == "state") {
      continue;
    }
    if (root != nullptr) {
      return nullptr;
    }
    root = &child;
  }
  return root;
}

std::string ParseStringLiteral(const Property& property) {
  const std::string& expression = property.expression;
  if (expression.size() < 2 || expression.front() != '"' || expression.back() != '"') {
    ThrowParseError(property.offset, property.line, property.column, "expected a double-quoted string");
  }
  std::string result;
  for (std::size_t index = 1; index + 1 < expression.size(); ++index) {
    if (expression[index] != '\\') {
      result.push_back(expression[index]);
      continue;
    }
    if (index + 1 >= expression.size()) {
      ThrowParseError(property.offset, property.line, property.column, "unterminated string escape");
    }
    switch (expression[index]) {
    case 'n':
      result.push_back('\n');
      break;
    case 'r':
      result.push_back('\r');
      break;
    case 't':
      result.push_back('\t');
      break;
    case '\\':
    case '"':
      result.push_back(expression[index]);
      break;
    default:
      ThrowParseError(property.offset, property.line, property.column, "unsupported string escape");
    }
  }
  return result;
}

} // namespace huxerui::declarative
