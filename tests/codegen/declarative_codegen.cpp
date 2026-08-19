#include "generator.h"

#include <catch2/catch_amalgamated.hpp>

#include <string>

namespace {

using huxerui::declarative_codegen::GenerateSources;
using huxerui::declarative_codegen::GeneratorError;

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

template <class Function> void RequireGeneratorError(Function&& function) {
  REQUIRE_THROWS_AS(function(), GeneratorError);
}

TEST_CASE("Declarative syntax rejects parent-size shortcuts") {
  RequireGeneratorError([] {
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
  RequireGeneratorError([] {
    static_cast<void>(GenerateSources(
        R"ui(
component Invalid {
  Text {}
}
)ui",
        "invalid"
    ));
  });
  RequireGeneratorError([] {
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
