#include <catch2/catch_test_macros.hpp>

#include "k7/common/diagnostics.h"

using namespace k7::common;

TEST_CASE("a diagnostic prints as file:line: message", "[diagnostics]")
{
    CHECK(Format(Diagnostic{ Severity::Error, "overrides/a.toml", 7, "unknown key 'x'" }) == "overrides/a.toml:7: unknown key 'x'");
    CHECK(Format(Diagnostic{ Severity::Warning, "config.toml", 0, "file not found" }) == "config.toml: file not found");
}
