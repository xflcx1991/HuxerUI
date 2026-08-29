#include "evaluator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace huxerui::declarative_preview {

namespace {

using huxerui::declarative::FindProperty;
using huxerui::declarative::Node;
using huxerui::declarative::ParseError;
using huxerui::declarative::ParseStringLiteral;
using huxerui::declarative::Property;
using huxerui::declarative::ThrowParseError;

[[noreturn]] void Error(const Property& property, std::string message) {
  ThrowParseError(property.offset, property.line, property.column, std::move(message));
}

enum class StateKind {
  Bool,
  Integer,
  Number,
  String,
};

[[nodiscard]] StateKind DetectKind(const std::string& literal) {
  if (literal == "true" || literal == "false") {
    return StateKind::Bool;
  }
  if (literal.size() >= 2 && literal.front() == '"' && literal.back() == '"') {
    return StateKind::String;
  }
  if (std::all_of(literal.begin(), literal.end(), [](unsigned char character) {
        return std::isdigit(character) != 0 || character == '.' || character == '-';
      })) {
    if (literal.find('.') != std::string::npos) {
      return StateKind::Number;
    }
    return StateKind::Integer;
  }
  return StateKind::String;
}

struct StateBinding {
  std::function<std::string()> read;
  std::function<void(const std::string&)> write;
};

using StateBindings = std::map<std::string, StateBinding>;
using SharedBindings = std::shared_ptr<StateBindings>;

class ExpressionEvaluator final {
public:
  ExpressionEvaluator(std::string_view expression, const StateBindings& bindings, std::size_t offset, int line,
                     int column)
      : expression_(expression), bindings_(bindings), offset_(offset), line_(line), column_(column) {
    Advance();
  }

  [[nodiscard]] std::string Evaluate() {
    const std::string result = ParseBinary(0);
    if (current_.kind != TokenKind::End) {
      RuntimeError("unexpected token '" + current_.text + "'");
    }
    return result;
  }

private:
  enum class TokenKind {
    End,
    Identifier,
    Number,
    String,
    Operator,
  };

  struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
  };

  [[nodiscard]] std::string ParseBinary(int minimum_precedence) {
    std::string left = ParseUnary();
    while (current_.kind == TokenKind::Operator) {
      const int precedence = OperatorPrecedence(current_.text);
      if (precedence < minimum_precedence) {
        break;
      }
      const std::string operation = current_.text;
      Advance();
      const std::string right = ParseBinary(precedence + 1);
      left = ApplyBinary(operation, left, right);
    }
    return left;
  }

  [[nodiscard]] std::string ParseUnary() {
    if (current_.kind == TokenKind::Operator && (current_.text == "!" || current_.text == "-")) {
      const std::string operation = current_.text;
      Advance();
      const std::string operand = ParseUnary();
      if (operation == "!") {
        return operand == "true" ? "false" : "true";
      }
      return "-" + operand;
    }
    if (current_.kind == TokenKind::Operator && current_.text == "(") {
      Advance();
      const std::string result = ParseBinary(0);
      Expect(")");
      return result;
    }
    if (current_.kind == TokenKind::Identifier) {
      const std::string identifier = current_.text;
      Advance();
      if (identifier == "true") {
        return "true";
      }
      if (identifier == "false") {
        return "false";
      }
      const auto found = bindings_.find(identifier);
      if (found == bindings_.end()) {
        RuntimeError("unknown state '" + identifier + "'");
      }
      return found->second.read();
    }
    if (current_.kind == TokenKind::Number) {
      const std::string number_text = current_.text;
      Advance();
      return number_text;
    }
    if (current_.kind == TokenKind::String) {
      const std::string string_text = current_.text;
      Advance();
      return string_text;
    }
    RuntimeError("expected a literal, state, or parenthesized expression");
  }

  [[nodiscard]] std::string ApplyBinary(
      const std::string& operation, const std::string& left, const std::string& right
  ) {
    if (operation == "&&") {
      return (left != "false" && left != "0" && left != "0.0" && !left.empty()) &&
                     (right != "false" && right != "0" && right != "0.0" && !right.empty())
                 ? "true"
                 : "false";
    }
    if (operation == "||") {
      return (left != "false" && left != "0" && left != "0.0" && !left.empty()) ||
                     (right != "false" && right != "0" && right != "0.0" && !right.empty())
                 ? "true"
                 : "false";
    }

    if (operation == "==" || operation == "!=") {
      const bool equal = left == right;
      return (operation == "==") ? (equal ? "true" : "false") : (equal ? "false" : "true");
    }
    if (LooksLikeNumber(left) && LooksLikeNumber(right)) {
      const double left_value = std::stod(left);
      const double right_value = std::stod(right);
      if (operation == "<") {
        return left_value < right_value ? "true" : "false";
      }
      if (operation == "<=") {
        return left_value <= right_value ? "true" : "false";
      }
      if (operation == ">") {
        return left_value > right_value ? "true" : "false";
      }
      if (operation == ">=") {
        return left_value >= right_value ? "true" : "false";
      }
      if (operation == "+" || operation == "-" || operation == "*" || operation == "/") {
        double result = 0.0;
        if (operation == "+") result = left_value + right_value;
        else if (operation == "-") result = left_value - right_value;
        else if (operation == "*") result = left_value * right_value;
        else if (operation == "/") {
          if (right_value == 0.0) {
            RuntimeError("division by zero");
          }
          result = left_value / right_value;
        }
        std::ostringstream stream;
        stream << result;
        return stream.str();
      }
    }
    RuntimeError("unsupported operator '" + operation + "' for operands");
  }

  [[nodiscard]] static bool LooksLikeNumber(std::string_view text) {
    if (text.empty()) {
      return false;
    }
    std::size_t index = 0;
    if (text[index] == '-') {
      ++index;
    }
    bool has_digit = false;
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
      has_digit = true;
      ++index;
    }
    if (index < text.size() && text[index] == '.') {
      ++index;
      while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
        has_digit = true;
        ++index;
      }
    }
    return has_digit && index == text.size();
  }

  void Expect(std::string_view text) {
    if (current_.kind != TokenKind::Operator || current_.text != text) {
      RuntimeError("expected '" + std::string(text) + "'");
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
    if (character == '"') {
      const std::size_t start = position_++;
      while (position_ < expression_.size() && expression_[position_] != '"') {
        if (expression_[position_] == '\\' && position_ + 1 < expression_.size()) {
          ++position_;
        }
        ++position_;
      }
      if (position_ >= expression_.size()) {
        RuntimeError("unterminated string literal");
      }
      ++position_;
      current_ = {TokenKind::String, std::string(expression_.substr(start, position_ - start))};
      return;
    }
    if (std::isdigit(static_cast<unsigned char>(character)) != 0 || character == '.') {
      const std::size_t start = position_++;
      while (position_ < expression_.size() &&
             (std::isdigit(static_cast<unsigned char>(expression_[position_])) != 0 || expression_[position_] == '.' ||
              expression_[position_] == 'e' || expression_[position_] == 'E' || expression_[position_] == '+' ||
              expression_[position_] == '-')) {
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
    RuntimeError("unsupported expression character");
  }

  [[nodiscard]] static int OperatorPrecedence(std::string_view operation) {
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

  [[noreturn]] void RuntimeError(std::string message) const {
    ThrowParseError(offset_, line_, column_, "expression: " + std::move(message));
  }

  std::string_view expression_;
  const StateBindings& bindings_;
  std::size_t offset_;
  int line_;
  int column_;
  std::size_t position_ = 0;
  Token current_;
};

[[nodiscard]] std::string EvaluateExpression(const Property& property, const StateBindings& bindings) {
  return ExpressionEvaluator(property.expression, bindings, property.offset, property.line, property.column).Evaluate();
}

[[nodiscard]] bool EvaluateBoolean(const Property& property, const StateBindings& bindings) {
  return EvaluateExpression(property, bindings) == "true";
}

[[nodiscard]] float EvaluateFloat(const Property& property, const StateBindings& bindings) {
  return std::stof(EvaluateExpression(property, bindings));
}

[[nodiscard]] std::string EvaluateText(const Property& property, const StateBindings& bindings) {
  const std::string literal = ParseStringLiteral(property);
  std::string result;
  for (std::size_t index = 0; index < literal.size();) {
    if (literal[index] != '$' || index + 1 >= literal.size() || literal[index + 1] != '{') {
      result.push_back(literal[index++]);
      continue;
    }
    const std::size_t end = literal.find('}', index + 2);
    if (end == std::string::npos || end == index + 2) {
      Error(property, "invalid '${...}' interpolation");
    }
    const std::string name = literal.substr(index + 2, end - index - 2);
    const auto found = bindings.find(name);
    if (found == bindings.end()) {
      Error(property, "unknown interpolated state '" + name + "'");
    }
    result += found->second.read();
    index = end + 1;
  }
  return result;
}

[[nodiscard]] std::string ParseStringOrIdentifier(const Property& property) {
  if (property.expression.size() >= 2 && property.expression.front() == '"') {
    return ParseStringLiteral(property);
  }
  return huxerui::declarative::Trim(property.expression);
}

[[nodiscard]] huxerui::MainAxisAlignment ParseMainAlignment(const Property& property) {
  const std::string value = ParseStringOrIdentifier(property);
  if (value == "start") return huxerui::MainAxisAlignment::Start;
  if (value == "center") return huxerui::MainAxisAlignment::Center;
  if (value == "end") return huxerui::MainAxisAlignment::End;
  if (value == "spaceBetween") return huxerui::MainAxisAlignment::SpaceBetween;
  if (value == "spaceAround") return huxerui::MainAxisAlignment::SpaceAround;
  if (value == "spaceEvenly") return huxerui::MainAxisAlignment::SpaceEvenly;
  Error(property, "unsupported main axis alignment '" + value + "'");
}

[[nodiscard]] huxerui::CrossAxisAlignment ParseCrossAlignment(const Property& property) {
  const std::string value = ParseStringOrIdentifier(property);
  if (value == "start") return huxerui::CrossAxisAlignment::Start;
  if (value == "center") return huxerui::CrossAxisAlignment::Center;
  if (value == "end") return huxerui::CrossAxisAlignment::End;
  if (value == "stretch") return huxerui::CrossAxisAlignment::Stretch;
  Error(property, "unsupported cross axis alignment '" + value + "'");
}

[[nodiscard]] huxerui::HorizontalAlignment ParseHorizontalAlignment(const Property& property) {
  const std::string value = ParseStringOrIdentifier(property);
  if (value == "start") return huxerui::HorizontalAlignment::Start;
  if (value == "center") return huxerui::HorizontalAlignment::Center;
  if (value == "end") return huxerui::HorizontalAlignment::End;
  if (value == "stretch") return huxerui::HorizontalAlignment::Stretch;
  Error(property, "unsupported horizontal alignment '" + value + "'");
}

[[nodiscard]] huxerui::VerticalAlignment ParseVerticalAlignment(const Property& property) {
  const std::string value = ParseStringOrIdentifier(property);
  if (value == "start") return huxerui::VerticalAlignment::Start;
  if (value == "center") return huxerui::VerticalAlignment::Center;
  if (value == "end") return huxerui::VerticalAlignment::End;
  if (value == "stretch") return huxerui::VerticalAlignment::Stretch;
  Error(property, "unsupported vertical alignment '" + value + "'");
}

void ApplyActionShared(SharedBindings bindings, const Property& property) {
  const std::size_t equals = property.expression.find('=');
  if (equals == std::string::npos ||
      (equals + 1 < property.expression.size() && property.expression[equals + 1] == '=')) {
    Error(property, "onClick must assign a state");
  }
  const std::string target = huxerui::declarative::Trim(property.expression.substr(0, equals));
  const auto found = bindings->find(target);
  if (found == bindings->end()) {
    Error(property, "onClick target must be a declared state");
  }
  const std::string value_expression =
      huxerui::declarative::Trim(property.expression.substr(equals + 1));
  if (value_expression.empty()) {
    Error(property, "onClick assignment is missing a value");
  }
  const std::string new_value =
      ExpressionEvaluator(value_expression, *bindings, property.offset, property.line, property.column).Evaluate();
  found->second.write(new_value);
}

class NodeEvaluator final {
public:
  explicit NodeEvaluator(SharedBindings bindings) : bindings_(std::move(bindings)) {}

  [[nodiscard]] huxerui::View Evaluate(const Node& node) {
    if (node.type == "Center") {
      RejectUnknownProperties(node, "Center", {});
      if (node.children.size() != 1) {
        ThrowParseError(node.offset, node.line, node.column, "Center requires exactly one child");
      }
      return huxerui::Stack {
        Evaluate(node.children.front()),
      }.With(huxerui::Align(huxerui::HorizontalAlignment::Center, huxerui::VerticalAlignment::Center));
    }
    if (node.type == "Text") {
      RejectUnknownProperties(node, "Text", {"text", "fontSize"});
      const Property* text = FindProperty(node, "text");
      if (text == nullptr) {
        ThrowParseError(node.offset, node.line, node.column, "Text requires a text property");
      }
      if (!node.children.empty()) {
        ThrowParseError(
            node.children.front().offset,
            node.children.front().line,
            node.children.front().column,
            "Text cannot contain child nodes"
        );
      }
      huxerui::View result = huxerui::Text(EvaluateText(*text, *bindings_));
      if (const Property* font_size = FindProperty(node, "fontSize")) {
        result = std::move(result).With(huxerui::FontSize{EvaluateFloat(*font_size, *bindings_)});
      }
      return result;
    }
    if (node.type == "Button") {
      RejectUnknownProperties(node, "Button", {"text", "fontSize", "enabled", "disabled", "onClick"});
      const Property* text = FindProperty(node, "text");
      if (text == nullptr) {
        ThrowParseError(node.offset, node.line, node.column, "Button requires a text property");
      }
      if (!node.children.empty()) {
        ThrowParseError(
            node.children.front().offset,
            node.children.front().line,
            node.children.front().column,
            "Button cannot contain child nodes"
        );
      }
      const std::string label = ParseStringLiteral(*text);
      huxerui::View view = huxerui::Button(label);
      if (const Property* on_click = FindProperty(node, "onClick")) {
        view = std::move(view).OnClick([bindings = bindings_, on_click] {
          ApplyActionShared(bindings, *on_click);
        });
      }
      if (const Property* enabled = FindProperty(node, "enabled")) {
        view = std::move(view).With(huxerui::Enabled{EvaluateBoolean(*enabled, *bindings_)});
      }
      if (const Property* disabled = FindProperty(node, "disabled")) {
        view = std::move(view).With(huxerui::Enabled{!EvaluateBoolean(*disabled, *bindings_)});
      }
      if (const Property* font_size = FindProperty(node, "fontSize")) {
        view = std::move(view).With(huxerui::FontSize{EvaluateFloat(*font_size, *bindings_)});
      }
      return view;
    }
    if (node.type == "Row" || node.type == "Column" || node.type == "Stack") {
      return EvaluateLayout(node);
    }
    ThrowParseError(node.offset, node.line, node.column, "unsupported node '" + node.type + "'");
  }

private:
  [[nodiscard]] huxerui::View EvaluateLayout(const Node& node) {
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
      ThrowParseError(node.offset, node.line, node.column, "Stack does not support spacing");
    }

    std::vector<huxerui::View> children;
    children.reserve(node.children.size());
    for (const Node& child : node.children) {
      children.push_back(Evaluate(child));
    }

    huxerui::View result = BuildLayoutView(node.type, children);
    if (const Property* spacing = FindProperty(node, "spacing")) {
      result = std::move(result).With(huxerui::Spacing{EvaluateFloat(*spacing, *bindings_)});
    }
    if (const Property* main = FindProperty(node, "mainAxisAlignment")) {
      result = std::move(result).With(huxerui::MainAlign{ParseMainAlignment(*main)});
    }
    if (const Property* cross = FindProperty(node, "crossAxisAlignment")) {
      result = std::move(result).With(huxerui::CrossAlign{ParseCrossAlignment(*cross)});
    }
    if (const Property* padding = FindProperty(node, "padding")) {
      result = std::move(result).With(huxerui::Padding(EvaluateFloat(*padding, *bindings_)));
    }
    if (const Property* grow = FindProperty(node, "grow")) {
      result = std::move(result).With(huxerui::Grow{EvaluateFloat(*grow, *bindings_)});
    }
    if (const Property* alignment = FindProperty(node, "alignment")) {
      result = std::move(result).With(
          huxerui::Align(ParseHorizontalAlignment(*alignment), ParseVerticalAlignment(*alignment))
      );
    }
    return result;
  }

  [[nodiscard]] static huxerui::View BuildLayoutView(
      const std::string& type, std::vector<huxerui::View>& children
  ) {
    if (type == "Row") {
      return BuildContainer<huxerui::Row>(children);
    }
    if (type == "Column") {
      return BuildContainer<huxerui::Column>(children);
    }
    return BuildContainer<huxerui::Stack>(children);
  }

  template <class LayoutType>
  [[nodiscard]] static huxerui::View BuildContainer(std::vector<huxerui::View>& children) {
    if (children.empty()) {
      return LayoutType{};
    }
    if (children.size() == 1) {
      return LayoutType{std::move(children.front())};
    }
    if (children.size() == 2) {
      return LayoutType{std::move(children[0]), std::move(children[1])};
    }
    if (children.size() == 3) {
      return LayoutType{std::move(children[0]), std::move(children[1]), std::move(children[2])};
    }
    if (children.size() == 4) {
      return LayoutType{std::move(children[0]), std::move(children[1]), std::move(children[2]), std::move(children[3])};
    }
    huxerui::View combined = LayoutType{
        std::move(children[0]),
        std::move(children[1]),
        std::move(children[2]),
        std::move(children[3]),
    };
    for (std::size_t index = 4; index < children.size(); ++index) {
      combined = LayoutType{std::move(combined), std::move(children[index])};
    }
    return combined;
  }

  static void RejectUnknownProperties(const Node& node, std::string_view type,
                                       std::initializer_list<std::string_view> allowed) {
    for (const Property& property : node.properties) {
      if (std::find(allowed.begin(), allowed.end(), property.name) == allowed.end()) {
        ThrowParseError(
            property.offset,
            property.line,
            property.column,
            "unsupported " + std::string(type) + " property '" + property.name + "'"
        );
      }
    }
  }

  SharedBindings bindings_;
};

template <class T> struct StateCellHolder {
  huxerui::State<T> state;

  explicit StateCellHolder(huxerui::State<T> state) : state(std::move(state)) {}

  [[nodiscard]] std::string ReadString() const {
    std::ostringstream stream;
    stream << std::boolalpha << state.Get();
    return stream.str();
  }

  void WriteString(const std::string& value) {
    if constexpr (std::is_same_v<T, bool>) {
      state = (value == "true");
    } else if constexpr (std::is_same_v<T, std::string>) {
      state = value;
    } else if constexpr (std::is_arithmetic_v<T>) {
      try {
        state = static_cast<T>(std::stold(value));
      } catch (const std::exception&) {
        // ignore conversion errors
      }
    }
  }
};

template <class T> [[nodiscard]] StateBinding CreateBinding(huxerui::State<T> state) {
  auto holder = std::make_shared<StateCellHolder<T>>(std::move(state));
  StateBinding binding;
  binding.read = [holder]() -> std::string { return holder->ReadString(); };
  binding.write = [holder](const std::string& value) { holder->WriteString(value); };
  return binding;
}

} // namespace

struct StateSpec {
  std::string name;
  StateKind kind;
  std::string literal;
};

huxerui::View EvaluateComponent(const huxerui::declarative::Component& component) {
  std::vector<StateSpec> state_specs;
  bool has_state = false;
  for (const Node& child : component.children) {
    if (child.type != "state") {
      continue;
    }
    if (has_state) {
      ThrowParseError(child.offset, child.line, child.column, "only one state block is allowed per component");
    }
    has_state = true;
    if (!child.children.empty()) {
      ThrowParseError(child.offset, child.line, child.column, "state blocks cannot contain child nodes");
    }
    if (child.properties.empty()) {
      ThrowParseError(child.offset, child.line, child.column, "state block must declare at least one state");
    }
    for (const Property& property : child.properties) {
      state_specs.push_back({property.name, DetectKind(property.expression), property.expression});
    }
  }

  const Node* root = component.FindRoot();
  if (root == nullptr) {
    ThrowParseError(component.offset, component.line, component.column, "a component must have one root node");
  }

  if (!has_state) {
    return NodeEvaluator(std::make_shared<StateBindings>()).Evaluate(*root);
  }

  return huxerui::Scope([root, state_specs = std::move(state_specs)]() -> huxerui::View {
    auto bindings = std::make_shared<StateBindings>();
    for (const StateSpec& spec : state_specs) {
      switch (spec.kind) {
      case StateKind::Bool: {
        const bool value = spec.literal == "true";
        (*bindings)[spec.name] = CreateBinding<bool>(huxerui::UseState(value));
        break;
      }
      case StateKind::Integer: {
        const int64_t value = std::stoll(spec.literal);
        (*bindings)[spec.name] = CreateBinding<int64_t>(huxerui::UseState(value));
        break;
      }
      case StateKind::Number: {
        const double value = std::stod(spec.literal);
        (*bindings)[spec.name] = CreateBinding<double>(huxerui::UseState(value));
        break;
      }
      case StateKind::String: {
        const std::string value = (spec.literal.size() >= 2 && spec.literal.front() == '"' && spec.literal.back() == '"') ? spec.literal.substr(1, spec.literal.size() - 2) : spec.literal;
        (*bindings)[spec.name] = CreateBinding<std::string>(huxerui::UseState(value));
        break;
      }
      }
    }
    return NodeEvaluator(bindings).Evaluate(*root);
  });
}

} // namespace huxerui::declarative_preview
