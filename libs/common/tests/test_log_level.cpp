#include <catch2/catch_test_macros.hpp>

#include "k7/common/log_level.h"

using k7::common::LogLevel;
using k7::common::ParseLogLevel;

TEST_CASE("ParseLogLevel accepts the documented values", "[log]")
{
    REQUIRE(ParseLogLevel("trace") == LogLevel::Trace);
    REQUIRE(ParseLogLevel("debug") == LogLevel::Debug);
    REQUIRE(ParseLogLevel("info") == LogLevel::Info);
    REQUIRE(ParseLogLevel("warn") == LogLevel::Warn);
    REQUIRE(ParseLogLevel("error") == LogLevel::Error);
}

TEST_CASE("ParseLogLevel is case-insensitive", "[log]")
{
    REQUIRE(ParseLogLevel("INFO") == LogLevel::Info);
    REQUIRE(ParseLogLevel("Trace") == LogLevel::Trace);
}

TEST_CASE("ParseLogLevel rejects anything else", "[log]")
{
    REQUIRE_FALSE(ParseLogLevel("").has_value());
    REQUIRE_FALSE(ParseLogLevel("warning").has_value());
    REQUIRE_FALSE(ParseLogLevel(" info").has_value());
    REQUIRE_FALSE(ParseLogLevel("verbose").has_value());
}

TEST_CASE("ToString round-trips through ParseLogLevel", "[log]")
{
    for (auto level : { LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error }) {
        REQUIRE(ParseLogLevel(k7::common::ToString(level)) == level);
    }
}

TEST_CASE("ToSpdlog maps levels one to one", "[log]")
{
    REQUIRE(k7::common::ToSpdlog(LogLevel::Trace) == spdlog::level::trace);
    REQUIRE(k7::common::ToSpdlog(LogLevel::Error) == spdlog::level::err);
}
