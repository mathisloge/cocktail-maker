#include <catch2/catch_test_macros.hpp>
import cm;
import std;
using namespace cm;

TEST_CASE("Recipe can be formatted.", "[recipe]")
{
    Recipe recipe;

    const auto formatted_str = std::format("Test={}", recipe);
    CHECK(formatted_str == "Test=Recipe(name=)");
}
