#include "evaluator.h"

#include <catch2/catch_amalgamated.hpp>

#include <string>
#include <string_view>

#include "runtime_test_support.h"

namespace {

using huxerui::declarative::ParseDocument;
using huxerui::declarative_preview::EvaluateComponent;
using huxerui::test::ContainsText;
using huxerui::test::FindPresentedTextRect;
using huxerui::test::PointerEvent;
using huxerui::test::PointerEventType;
using huxerui::test::Point;
using huxerui::test::Runtime;
using huxerui::test::TestPlatform;

// Scope factories defer evaluation, so the parsed document must outlive Runtime.
huxerui::declarative::Document g_document;
huxerui::View g_root_view;

huxerui::View PreviewRoot() {
  return g_root_view;
}

class PreviewRuntime final {
public:
  explicit PreviewRuntime(std::string_view source)
      : document_(ParseDocument(source, "counter.ui")),
        runtime_(PreviewRoot, platform_) {
    g_root_view = EvaluateComponent(document_.components.front());
    runtime_.SetWindowMetrics({.viewport = {800.0F, 600.0F}});
    runtime_.BuildFrame();
  }

  Runtime& runtime() noexcept {
    return runtime_;
  }

private:
  TestPlatform platform_;
  huxerui::declarative::Document document_;
  Runtime runtime_;
};

void ClickText(Runtime& runtime, std::string_view label, std::int64_t pointer_id) {
  const auto rect = FindPresentedTextRect(runtime.BuildFrame(), label);
  REQUIRE(rect.has_value());

  const Point center{rect->x + rect->width / 2.0F, rect->y + rect->height / 2.0F};
  UNSCOPED_INFO("clicking '" << label << "' at " << center.x << ',' << center.y);
  runtime.HandlePointerEvent({PointerEventType::Down, pointer_id, center});
  runtime.HandlePointerEvent({PointerEventType::Up, pointer_id, center});
}

TEST_CASE("Declarative preview evaluates subtraction without consuming the operator") {
  PreviewRuntime preview(R"ui(
component Counter {
  state {
    count: 10
  }

  Center {
    Row {
      spacing: 12
      crossAxisAlignment: center

      Button {
        text: "-"
        onClick: count = count - 0
      }
      Text {
        text: "${count}"
        fontSize: 30
      }
      Button {
        text: "+"
        onClick: count = count + 1
      }
    }
  }
}
)ui");

  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "10"));

  ClickText(preview.runtime(), "-", 1);
  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "10"));

  ClickText(preview.runtime(), "+", 2);
  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "11"));
}

TEST_CASE("Declarative preview evaluates unary and boolean expressions") {
  PreviewRuntime preview(R"ui(
component Expressions {
  state {
    value: 0
    enabled: false
  }

  Center {
    Column {
      spacing: 12

      Button {
        text: "negative"
        onClick: value = 1 - -2
      }
      Button {
        text: "boolean"
        onClick: enabled = !(value > 0)
      }
      Text {
        text: "${value}"
      }
      Text {
        text: "${enabled}"
      }
    }
  }
}
)ui");

  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "0"));
  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "false"));

  ClickText(preview.runtime(), "boolean", 1);
  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "true"));

  ClickText(preview.runtime(), "negative", 2);
  REQUIRE(ContainsText(preview.runtime().BuildFrame(), "3"));
}

} // namespace
