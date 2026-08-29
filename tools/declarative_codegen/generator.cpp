#include "generator.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace huxerui::declarative_codegen {

namespace {

using huxerui::declarative::Component;
using huxerui::declarative::Document;
using huxerui::declarative::FindProperty;
using huxerui::declarative::Node;
using huxerui::declarative::ParseDocument;
using huxerui::declarative::ParseError;
using huxerui::declarative::ParseStringLiteral;
using huxerui::declarative::Property;
using huxerui::declarative::ThrowParseError;
using huxerui::declarative::Trim;

using StatesMap = std::map<std::string, std::string>;

[[noreturn]] void Error(const Property& property, std::string message) {
  ThrowParseError(property.offset, property.line, property.column, std::move(message));
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

[[nodiscard]] std::string TextExpression(const Property& property, const StatesMap& states) {
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
      Error(property, "invalid '${...}' interpolation");
    }
    const std::string name = value.substr(index + 2, end - index - 2);
    if (states.find(name) == states.end()) {
      Error(property, "unknown interpolated state '" + name + "'");
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

[[nodiscard]] std::string Expression(const Property& property, const StatesMap& states) {
  return huxerui::declarative::ExpressionParser(property.expression, states, property.offset, property.line, property.column)
      .Parse();
}

[[nodiscard]] std::string NumericModifier(const Property& property, const StatesMap& states, std::string_view type) {
  const std::string value = "static_cast<float>(" + Expression(property, states) + ")";
  if (type == "Padding") {
    return std::string(type) + "(" + value + ")";
  }
  return std::string(type) + "{" + value + "}";
}

[[nodiscard]] std::string
Alignment(std::string_view value, std::string_view enum_name, const Property& property,
          std::initializer_list<std::string_view> allowed) {
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
    Error(property, "unsupported alignment '" + std::string(value) + "'");
  }
  return std::string(enum_name) + "::" + found->second;
}

[[nodiscard]] std::string Action(const Property& property, const StatesMap& states) {
  const std::string expression = property.expression;
  const std::size_t equals = expression.find('=');
  if (equals == std::string::npos || (equals + 1 < expression.size() && expression[equals + 1] == '=')) {
    Error(property, "onClick must assign a state");
  }
  const std::string target = Trim(expression.substr(0, equals));
  if (target.empty() || states.find(target) == states.end()) {
    Error(property, "onClick target must be a declared state");
  }
  const std::string value = Trim(expression.substr(equals + 1));
  if (value.empty()) {
    Error(property, "onClick assignment is missing a value");
  }
  const std::string parsed =
      huxerui::declarative::ExpressionParser(value, states, property.offset, property.line, property.column).Parse();
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
        ThrowParseError(child.offset, child.line, child.column, "state blocks cannot contain child nodes");
      }
      for (const Property& property : child.properties) {
        if (states_.contains(property.name)) {
          ThrowParseError(
              property.offset,
              property.line,
              property.column,
              "state '" + property.name + "' is declared more than once"
          );
        }
        states_.insert_or_assign(property.name, "auto");
      }
      if (child.properties.empty()) {
        ThrowParseError(child.offset, child.line, child.column, "state block must declare at least one state");
      }
    }
  }

  [[nodiscard]] std::string EmitFunction() const {
    const Node* root = component_.FindRoot();
    if (root == nullptr) {
      ThrowParseError(component_.offset, component_.line, component_.column, "a component must have one root node");
    }

    std::ostringstream output;
    output << "View " << component_.name << "() {\n";
    if (!states_.empty()) {
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
    } else {
      const std::string emitted_root = Indent(EmitNode(*root), 2);
      output << "  return " << emitted_root.substr(2) << ";\n";
    }
    output << "}\n";
    return output.str();
  }

private:
  [[nodiscard]] std::string EmitNode(const Node& node) const {
    if (node.type == "Center") {
      RejectUnknownProperties(node, "Center", {});
      if (node.children.size() != 1) {
        ThrowParseError(node.offset, node.line, node.column, "Center requires exactly one child");
      }
      return "Stack {\n" + Indent(EmitNode(node.children.front()), 2) +
             "\n}.With(\n    Align(HorizontalAlignment::Center, VerticalAlignment::Center)\n)";
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
        ThrowParseError(node.offset, node.line, node.column, "Stack does not support spacing");
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
    ThrowParseError(node.offset, node.line, node.column, "unsupported node '" + node.type + "'");
  }

  [[nodiscard]] static std::string ParseStringOrIdentifier(const Property& property) {
    if (property.expression.size() >= 2 && property.expression.front() == '"') {
      return ParseStringLiteral(property);
    }
    return Trim(property.expression);
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
  StatesMap states_;
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

GeneratedSources
GenerateSources(std::string_view input, std::string_view source_path, std::string_view namespace_name) {
  const Document document = ParseDocument(input, source_path);
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
