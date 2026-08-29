#include "generator.h"

#include <catch2/catch_amalgamated.hpp>

#include <string>

namespace {

using huxerui::declarative_codegen::GenerateSources;
using huxerui::declarative::ParseError;

TEST_CASE("Declarative counter emits existing HuxerUI composition APIs") {
  const std::string source = R"ui(
component Counter {
  state {
    count: 0
  }
  Center {
    Row {
      spacing: 12
      crossAxisAlignment: center
      Button {
        text: "+"
        onClick: count = count + 1
      }
      Text {
        text: "${count}"
        fontSize: 30
      }
    }
  }
}
)ui";

  const auto generated = GenerateSources(source, "counter");
  REQUIRE(generated.header.find("View Counter();") != std::string::npos);
  REQUIRE(generated.source.find("auto count = UseState(0);") != std::string::npos);
  REQUIRE(generated.source.find("Text::Format(\"{}\", count)") != std::string::npos);
  REQUIRE(generated.source.find("count = (count + 1);") != std::string::npos);
  REQUIRE(generated.source.find("Align(HorizontalAlignment::Center, VerticalAlignment::Center)") != std::string::npos);
}

TEST_CASE("Declarative state names are not limited to count") {
  const auto generated = GenerateSources(
      R"ui(
component Toggle {
  state {
    enabled: true
  }
  Button {
    text: "toggle"
    enabled: enabled
    onClick: enabled = !enabled
  }
}
)ui",
      "toggle"
  );

  REQUIRE(generated.source.find("auto enabled = UseState(true);") != std::string::npos);
  REQUIRE(generated.source.find("enabled = (!enabled);") != std::string::npos);
}

template <class Function> void RequireParseError(Function&& function) {
  REQUIRE_THROWS_AS(function(), ParseError);
}

TEST_CASE("Declarative expressions preserve binary and unary operators") {
  const auto generated = GenerateSources(
      R"ui(
component Counter {
  state {
    count: 0
    enabled: false
  }
  Column {
    Button {
      text: "subtract"
      onClick: count = count - 0
    }
    Button {
      text: "negative"
      onClick: count = 1 - -2
    }
    Button {
      text: "toggle"
      onClick: enabled = !(count > 0)
    }
    Button {
      text: "precedence"
      onClick: count = count + 2 * 3
    }
  }
}
)ui",
      "counter"
  );

  REQUIRE(generated.source.find("count = (count - 0);") != std::string::npos);
  REQUIRE(generated.source.find("count = (1 - (-2));") != std::string::npos);
  REQUIRE(generated.source.find("enabled = (!((count > 0)));") != std::string::npos);
  REQUIRE(generated.source.find("count = (count + (2 * 3));") != std::string::npos);
}

TEST_CASE("Declarative syntax rejects parent-size shortcuts") {
  RequireParseError([] {
    static_cast<void>(GenerateSources(
        R"ui(
component Invalid {
  Text {
    text: "${screen.width}"
  }
}
)ui",
        "invalid"
    ));
  });
}

TEST_CASE("Declarative syntax validates component roots and required properties") {
  RequireParseError([] {
    static_cast<void>(GenerateSources(
        R"ui(
component Invalid {
  Text {}
}
)ui",
        "invalid"
    ));
  });
  RequireParseError([] {
    static_cast<void>(GenerateSources(
        R"ui(
component Invalid {
  Text { text: "one" }
  Text { text: "two" }
}
)ui",
        "invalid"
    ));
  });
}

} // namespace
