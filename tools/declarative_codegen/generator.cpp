#include "generator.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace huxerui::declarative_codegen {
namespace {

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
};

struct Document {
  std::vector<Component> components;
};

[[nodiscard]] std::string Trim(std::string value) {
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

class Parser final {
public:
  Parser(std::string_view source, std::string_view source_path) : source_(source), source_path_(source_path) {}

  [[nodiscard]] Document Parse() {
    Document document;
    SkipTrivia();
    while (!End()) {
      document.components.push_back(ParseComponent());
      SkipTrivia();
    }
    if (document.components.empty()) {
      Fail("expected at least one component declaration");
    }
    return document;
  }

private:
  [[nodiscard]] Component ParseComponent() {
    const std::size_t offset = position_;
    const int line = line_;
    const int column = column_;
    const std::string keyword = ParseIdentifier();
    if (keyword != "component") {
      Fail("expected 'component' declaration at module scope", offset, line, column);
    }
    SkipInlineSpace();
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
    return Trim(std::string(source_.substr(start, position_ - start)));
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
    std::string full_message = source_path_.empty() ? std::move(message) : source_path_ + ": " + std::move(message);
    throw GeneratorError(offset, line, column, std::move(full_message));
  }

  std::string_view source_;
  std::string source_path_;
  std::size_t position_ = 0;
  int line_ = 1;
  int column_ = 1;
};

enum class TokenKind {
  End,
  Identifier,
  Number,
  Operator,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
};

class ExpressionParser final {
public:
  ExpressionParser(
      std::string_view expression,
      const std::map<std::string, std::string>& states,
      std::size_t offset = 0,
      int line = 1,
      int column = 1
  )
      : expression_(expression), states_(states), offset_(offset), line_(line), column_(column) {
    Advance();
  }

  [[nodiscard]] std::string Parse() {
    const std::string result = ParseBinary(0);
    if (current_.kind != TokenKind::End) {
      Error("unexpected token '" + current_.text + "'");
    }
    return result;
  }

private:
  [[nodiscard]] std::string ParseBinary(int minimum_precedence) {
    std::string left = ParseUnary();
    while (current_.kind == TokenKind::Operator) {
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

  [[nodiscard]] std::string ParseUnary() {
    if (current_.kind == TokenKind::Operator && (current_.text == "!" || current_.text == "-")) {
      const std::string operation = current_.text;
      Advance();
      return "(" + operation + ParseUnary() + ")";
    }
    if (current_.kind == TokenKind::Operator && current_.text == "(") {
      Advance();
      const std::string result = ParseBinary(0);
      Expect(")");
      return "(" + result + ")";
    }
    if (current_.kind == TokenKind::Identifier) {
      const std::string identifier = current_.text;
      Advance();
      if (identifier != "true" && identifier != "false" && states_.find(identifier) == states_.end()) {
        Error("unknown state '" + identifier + "'");
      }
      return identifier;
    }
    if (current_.kind == TokenKind::Number) {
      const std::string number = current_.text;
      Advance();
      return number;
    }
    Error("expected a literal, state, or parenthesized expression");
  }

  void Expect(std::string_view text) {
    if (current_.kind != TokenKind::Operator || current_.text != text) {
      Error("expected '" + std::string(text) + "'");
    }
    Advance();
  }

  void Advance() {
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
      current_ = {TokenKind::Identifier, std::string(expression_.substr(start, position_ - start))};
      return;
    }
    if (std::isdigit(static_cast<unsigned char>(character)) != 0 || character == '.') {
      const std::size_t start = position_++;
      while (position_ < expression_.size() &&
             (std::isdigit(static_cast<unsigned char>(expression_[position_])) || expression_[position_] == '.')) {
        ++position_;
      }
      current_ = {TokenKind::Number, std::string(expression_.substr(start, position_ - start))};
      return;
    }
    const std::string two_character =
        position_ + 1 < expression_.size() ? std::string(expression_.substr(position_, 2)) : std::string{};
    if (two_character == "<=" || two_character == ">=" || two_character == "==" || two_character == "!=" ||
        two_character == "&&" || two_character == "||") {
      position_ += 2;
      current_ = {TokenKind::Operator, two_character};
      return;
    }
    if (std::string_view("+-*/!<>()").find(character) != std::string_view::npos) {
      ++position_;
      current_ = {TokenKind::Operator, std::string(1, character)};
      return;
    }
    Error("unsupported expression character");
  }

  [[nodiscard]] static int Precedence(std::string_view operation) {
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

  [[noreturn]] void Error(std::string message) const {
    throw GeneratorError(offset_, line_, column_, "expression: " + std::move(message));
  }

  std::string_view expression_;
  const std::map<std::string, std::string>& states_;
  std::size_t offset_;
  int line_;
  int column_;
  std::size_t position_ = 0;
  Token current_;
};

[[nodiscard]] const Property* FindProperty(const Node& node, std::string_view name) {
  const auto found = std::find_if(node.properties.begin(), node.properties.end(), [&](const Property& property) {
    return property.name == name;
  });
  return found == node.properties.end() ? nullptr : &*found;
}

void RejectUnknownProperties(const Node& node, std::string_view type, std::initializer_list<std::string_view> allowed) {
  for (const Property& property : node.properties) {
    if (std::find(allowed.begin(), allowed.end(), property.name) == allowed.end()) {
      throw GeneratorError(
          property.offset,
          property.line,
          property.column,
          "unsupported " + std::string(type) + " property '" + property.name + "'"
      );
    }
  }
}

[[nodiscard]] std::string ParseStringLiteral(const Property& property) {
  const std::string& expression = property.expression;
  if (expression.size() < 2 || expression.front() != '"' || expression.back() != '"') {
    throw GeneratorError(property.offset, property.line, property.column, "expected a double-quoted string");
  }
  std::string result;
  for (std::size_t index = 1; index + 1 < expression.size(); ++index) {
    if (expression[index] != '\\') {
      result.push_back(expression[index]);
      continue;
    }
    if (index + 1 >= expression.size()) {
      throw GeneratorError(property.offset, property.line, property.column, "unterminated string escape");
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
      throw GeneratorError(property.offset, property.line, property.column, "unsupported string escape");
    }
  }
  return result;
}

[[nodiscard]] std::string CppString(std::string_view value) {
  std::string result = "\"";
  for (const char character : value) {
    switch (character) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result.push_back(character);
      break;
    }
  }
  result += '"';
  return result;
}

[[nodiscard]] std::string TextExpression(const Property& property, const std::map<std::string, std::string>& states) {
  const std::string value = ParseStringLiteral(property);
  std::string format;
  std::vector<std::string> arguments;
  for (std::size_t index = 0; index < value.size();) {
    if (value[index] != '$' || index + 1 >= value.size() || value[index + 1] != '{') {
      format.push_back(value[index++]);
      continue;
    }
    const std::size_t end = value.find('}', index + 2);
    if (end == std::string::npos || end == index + 2) {
      throw GeneratorError(property.offset, property.line, property.column, "invalid '${...}' interpolation");
    }
    const std::string name = value.substr(index + 2, end - index - 2);
    if (states.find(name) == states.end()) {
      throw GeneratorError(
          property.offset,
          property.line,
          property.column,
          "unknown interpolated state '" + name + "'"
      );
    }
    format += "{}";
    arguments.push_back(name);
    index = end + 1;
  }
  if (arguments.empty()) {
    return "Text(" + CppString(format) + ")";
  }
  std::string result = "Text::Format(" + CppString(format);
  for (const std::string& argument : arguments) {
    result += ", " + argument;
  }
  result += ")";
  return result;
}

[[nodiscard]] std::string Expression(const Property& property, const std::map<std::string, std::string>& states) {
  return ExpressionParser(property.expression, states, property.offset, property.line, property.column).Parse();
}

[[nodiscard]] std::string
NumericModifier(const Property& property, const std::map<std::string, std::string>& states, std::string_view type) {
  const std::string value = "static_cast<float>(" + Expression(property, states) + ")";
  if (type == "Padding") {
    return std::string(type) + "(" + value + ")";
  }
  return std::string(type) + "{" + value + "}";
}

[[nodiscard]] std::string Alignment(
    std::string_view value,
    std::string_view enum_name,
    const Property& property,
    std::initializer_list<std::string_view> allowed
) {
  static const std::map<std::string, std::string> values{
      {"start", "Start"},
      {"center", "Center"},
      {"end", "End"},
      {"stretch", "Stretch"},
      {"spaceBetween", "SpaceBetween"},
      {"spaceAround", "SpaceAround"},
      {"spaceEvenly", "SpaceEvenly"},
  };
  const auto found = values.find(std::string(value));
  if (found == values.end() || std::find(allowed.begin(), allowed.end(), value) == allowed.end()) {
    throw GeneratorError(
        property.offset,
        property.line,
        property.column,
        "unsupported alignment '" + std::string(value) + "'"
    );
  }
  return std::string(enum_name) + "::" + found->second;
}

[[nodiscard]] std::string Action(const Property& property, const std::map<std::string, std::string>& states) {
  const std::string expression = property.expression;
  const std::size_t equals = expression.find('=');
  if (equals == std::string::npos || (equals + 1 < expression.size() && expression[equals + 1] == '=')) {
    throw GeneratorError(property.offset, property.line, property.column, "onClick must assign a state");
  }
  const std::string target = Trim(expression.substr(0, equals));
  if (target.empty() || states.find(target) == states.end()) {
    throw GeneratorError(property.offset, property.line, property.column, "onClick target must be a declared state");
  }
  const std::string value = Trim(expression.substr(equals + 1));
  if (value.empty()) {
    throw GeneratorError(property.offset, property.line, property.column, "onClick assignment is missing a value");
  }
  const std::string parsed = ExpressionParser(value, states, property.offset, property.line, property.column).Parse();
  return target + " = " + parsed + ";";
}

class Emitter final {
public:
  explicit Emitter(const Component& component) : component_(component) {
    for (const Node& child : component.children) {
      if (child.type != "state") {
        continue;
      }
      if (!child.children.empty()) {
        throw GeneratorError(child.offset, child.line, child.column, "state blocks cannot contain child nodes");
      }
      for (const Property& property : child.properties) {
        if (states_.contains(property.name)) {
          throw GeneratorError(
              property.offset,
              property.line,
              property.column,
              "state '" + property.name + "' is declared more than once"
          );
        }
        states_.insert_or_assign(property.name, "auto");
      }
      if (child.properties.empty()) {
        throw GeneratorError(child.offset, child.line, child.column, "state block must declare at least one state");
      }
    }
  }

  [[nodiscard]] std::string EmitFunction() const {
    const Node* root = nullptr;
    for (const Node& child : component_.children) {
      if (child.type == "state") {
        continue;
      }
      if (root != nullptr) {
        throw GeneratorError(child.offset, child.line, child.column, "a component must have one root node");
      }
      root = &child;
    }
    if (root == nullptr) {
      throw GeneratorError(component_.offset, component_.line, component_.column, "component has no root node");
    }

    std::ostringstream output;
    output << "View " << component_.name << "() {\n";
    output << "  return Scope([]() -> View {\n";
    for (const Node& child : component_.children) {
      if (child.type != "state") {
        continue;
      }
      for (const Property& property : child.properties) {
        output << "    auto " << property.name << " = UseState(" << Expression(property, {}) << ");\n";
      }
    }
    const std::string emitted_root = Indent(EmitNode(*root), 4);
    output << "    return " << emitted_root.substr(4) << ";\n";
    output << "  });\n";
    output << "}\n";
    return output.str();
  }

private:
  [[nodiscard]] std::string EmitNode(const Node& node) const {
    if (node.type == "Center") {
      RejectUnknownProperties(node, "Center", {});
      if (node.children.size() != 1) {
        throw GeneratorError(node.offset, node.line, node.column, "Center requires exactly one child");
      }
      return "Stack {\n" + Indent(EmitNode(node.children.front()), 2) +
             "\n}.With(\n    Align(HorizontalAlignment::Center, VerticalAlignment::Center)\n)";
    }
    if (node.type == "Text") {
      RejectUnknownProperties(node, "Text", {"text", "fontSize"});
      const Property* text = FindProperty(node, "text");
      if (text == nullptr) {
        throw GeneratorError(node.offset, node.line, node.column, "Text requires a text property");
      }
      if (!node.children.empty()) {
        throw GeneratorError(
            node.children.front().offset,
            node.children.front().line,
            node.children.front().column,
            "Text cannot contain child nodes"
        );
      }
      std::string result = TextExpression(*text, states_);
      if (const Property* font_size = FindProperty(node, "fontSize")) {
        result += ".With(" + NumericModifier(*font_size, states_, "FontSize") + ")";
      }
      return result;
    }
    if (node.type == "Button") {
      RejectUnknownProperties(node, "Button", {"text", "fontSize", "enabled", "disabled", "onClick"});
      const Property* text = FindProperty(node, "text");
      if (text == nullptr) {
        throw GeneratorError(node.offset, node.line, node.column, "Button requires a text property");
      }
      if (!node.children.empty()) {
        throw GeneratorError(
            node.children.front().offset,
            node.children.front().line,
            node.children.front().column,
            "Button cannot contain child nodes"
        );
      }
      const std::string label = ParseStringLiteral(*text);
      std::string result = "Button(" + CppString(label) + ")";
      if (const Property* on_click = FindProperty(node, "onClick")) {
        std::string captures;
        for (const auto& [name, type] : states_) {
          static_cast<void>(type);
          if (!captures.empty())
            captures += ", ";
          captures += name;
        }
        result += ".OnClick([" + captures + "] { " + Action(*on_click, states_) + " })";
      }
      std::vector<std::string> modifiers;
      if (const Property* enabled = FindProperty(node, "enabled")) {
        modifiers.push_back("Enabled{static_cast<bool>(" + Expression(*enabled, states_) + ")}");
      }
      if (const Property* disabled = FindProperty(node, "disabled")) {
        modifiers.push_back("Enabled{!(static_cast<bool>(" + Expression(*disabled, states_) + "))}");
      }
      if (const Property* font_size = FindProperty(node, "fontSize")) {
        modifiers.push_back(NumericModifier(*font_size, states_, "FontSize"));
      }
      return AddModifiers(result, modifiers);
    }
    if (node.type == "Row" || node.type == "Column" || node.type == "Stack") {
      const bool is_stack = node.type == "Stack";
      const std::initializer_list<std::string_view> allowed =
          is_stack ? std::initializer_list<std::string_view>{"alignment", "padding"}
                   : std::initializer_list<std::string_view>{
                         "spacing",
                         "mainAxisAlignment",
                         "crossAxisAlignment",
                         "padding",
                         "grow"
                     };
      RejectUnknownProperties(node, node.type, allowed);
      if (is_stack && FindProperty(node, "spacing") != nullptr) {
        throw GeneratorError(node.offset, node.line, node.column, "Stack does not support spacing");
      }
      if (node.type == "Row" || node.type == "Column") {
        if (const Property* main = FindProperty(node, "mainAxisAlignment")) {
          static_cast<void>(Alignment(
              ParseStringOrIdentifier(*main),
              "MainAxisAlignment",
              *main,
              {"start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly"}
          ));
        }
        if (const Property* cross = FindProperty(node, "crossAxisAlignment")) {
          static_cast<void>(Alignment(
              ParseStringOrIdentifier(*cross),
              "CrossAxisAlignment",
              *cross,
              {"start", "center", "end", "stretch"}
          ));
        }
      }
      std::ostringstream result;
      result << node.type << " {\n";
      for (std::size_t index = 0; index < node.children.size(); ++index) {
        result << Indent(EmitNode(node.children[index]), 2);
        if (index + 1 < node.children.size())
          result << ',';
        result << '\n';
      }
      result << '}';
      std::vector<std::string> modifiers;
      if (const Property* spacing = FindProperty(node, "spacing")) {
        modifiers.push_back(NumericModifier(*spacing, states_, "Spacing"));
      }
      if (const Property* main = FindProperty(node, "mainAxisAlignment")) {
        modifiers.push_back(
            "MainAlign{" +
            Alignment(
                ParseStringOrIdentifier(*main),
                "MainAxisAlignment",
                *main,
                {"start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly"}
            ) +
            "}"
        );
      }
      if (const Property* cross = FindProperty(node, "crossAxisAlignment")) {
        modifiers.push_back(
            "CrossAlign{" +
            Alignment(
                ParseStringOrIdentifier(*cross),
                "CrossAxisAlignment",
                *cross,
                {"start", "center", "end", "stretch"}
            ) +
            "}"
        );
      }
      if (const Property* padding = FindProperty(node, "padding")) {
        modifiers.push_back(NumericModifier(*padding, states_, "Padding"));
      }
      if (const Property* grow = FindProperty(node, "grow")) {
        modifiers.push_back(NumericModifier(*grow, states_, "Grow"));
      }
      if (const Property* alignment = FindProperty(node, "alignment")) {
        const std::string value = ParseStringOrIdentifier(*alignment);
        const std::initializer_list<std::string_view> stack_values{"start", "center", "end", "stretch"};
        const std::string mapped = Alignment(value, "HorizontalAlignment", *alignment, stack_values);
        const std::string vertical = Alignment(value, "VerticalAlignment", *alignment, stack_values);
        modifiers.push_back("Align{" + mapped + ", " + vertical + "}");
      }
      return AddModifiers(result.str(), modifiers);
    }
    throw GeneratorError(node.offset, node.line, node.column, "unsupported node '" + node.type + "'");
  }

  [[nodiscard]] static std::string ParseStringOrIdentifier(const Property& property) {
    if (property.expression.size() >= 2 && property.expression.front() == '"') {
      return ParseStringLiteral(property);
    }
    return Trim(property.expression);
  }

  [[nodiscard]] static std::string AddModifiers(std::string result, const std::vector<std::string>& modifiers) {
    if (modifiers.empty())
      return result;
    result += ".With(\n";
    for (std::size_t index = 0; index < modifiers.size(); ++index) {
      result += "    " + modifiers[index];
      if (index + 1 < modifiers.size())
        result += ',';
      result += '\n';
    }
    result += ')';
    return result;
  }

  [[nodiscard]] static std::string Indent(std::string value, int spaces) {
    const std::string prefix(static_cast<std::size_t>(spaces), ' ');
    std::string result;
    result.reserve(value.size() + prefix.size() * 4);
    bool line_start = true;
    for (const char character : value) {
      if (line_start) {
        result += prefix;
        line_start = false;
      }
      result.push_back(character);
      if (character == '\n')
        line_start = true;
    }
    return result;
  }

  const Component& component_;
  std::map<std::string, std::string> states_;
};

[[nodiscard]] std::string Header(const Document& document, std::string_view namespace_name) {
  std::ostringstream output;
  output << "#pragma once\n\n#include <huxerui/huxerui.h>\n\nnamespace " << namespace_name
         << " {\n\nusing huxerui::View;\n\n";
  for (const Component& component : document.components) {
    output << "View " << component.name << "();\n";
  }
  output << "\n} // namespace " << namespace_name << "\n";
  return output.str();
}

} // namespace

GeneratorError::GeneratorError(std::size_t offset, int line, int column, std::string message)
    : std::runtime_error(std::move(message)), offset_(offset), line_(line), column_(column) {}

GeneratedSources
GenerateSources(std::string_view input, std::string_view source_path, std::string_view namespace_name) {
  const Document document = Parser(input, source_path).Parse();
  GeneratedSources result;
  result.header = Header(document, namespace_name);
  result.source = "#include \"" + std::string(source_path) + ".generated.h\"\n\nnamespace " +
                  std::string(namespace_name) + " {\n\nusing namespace huxerui;\n\n";
  for (const Component& component : document.components) {
    result.source += Emitter(component).EmitFunction();
    result.source += '\n';
  }
  result.source += "} // namespace " + std::string(namespace_name) + "\n";
  return result;
}

} // namespace huxerui::declarative_codegen
