#include <catch2/catch_test_macros.hpp>

#include "core/carry.h"

using namespace k7::rscw::core;

TEST_CASE("a value below min_total is raised to min_total", "[carry]")
{
    CHECK(ClampValue(10, 20) == 20);
    CHECK(ClampValue(20, 20) == 20);
    CHECK(ClampValue(115, 20) == 115);
}

TEST_CASE("the contribution is counted from the base of the race", "[carry]")
{
    CHECK(Contribution(115, 20, 300.0f) == -185.0f);
    CHECK(Contribution(300, 20, 300.0f) == 0.0f);
    CHECK(Contribution(400, 20, 300.0f) == 100.0f);
    CHECK(Contribution(5, 20, 300.0f) == -280.0f);
    // A race whose record a mod changed: still the configured value in the end.
    CHECK(Contribution(115, 20, 250.0f) == -135.0f);
}

TEST_CASE("a race that is not in the configuration contributes nothing", "[carry]")
{
    CHECK(Contribution(std::nullopt, 20, 300.0f) == 0.0f);
    CHECK(Contribution(std::nullopt, 20, 250.0f) == 0.0f);
}

TEST_CASE("a recount applies only the difference and is idempotent", "[carry]")
{
    const float applied = Contribution(115, 20, 300.0f);
    CHECK(Delta(applied, applied) == 0.0f);
    CHECK(Delta(applied, Contribution(120, 20, 300.0f)) == 5.0f);
    CHECK(Delta(0.0f, applied) == applied);
    CHECK(Delta(applied, 0.0f) == -applied);
}
