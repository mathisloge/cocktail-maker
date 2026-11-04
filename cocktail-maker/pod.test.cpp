#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <proto/field/ErrorCodeCommon.h>
import std;
import cm.core;
import cm;

using namespace std::chrono_literals;

TEST_CASE("classify_dispense tells why a pour ended", "[pod]")
{
    using enum cm::DispenseOutcome;
    const auto [requested, measured, elapsed, expected] =
        GENERATE(table<double, double, std::chrono::milliseconds, cm::DispenseOutcome>({
            {20, 20, 5s, completed},
            {20, 19, 5s, completed},
            {20, 18.6, 5s, completed},
            {20, 18.4, 5s, bottle_empty},
            {20, 18, 5s, bottle_empty},
            {20, 25, 2s, completed},
            {20, 0, 6s, bottle_empty},
            {170, 150, 31s, timed_out},
            {20, 10, 30s, timed_out},
            {20, 10, 29999ms, bottle_empty},
        }));

    CHECK(cm::classify_dispense(requested * cm::units::milli_litre, measured * cm::units::milli_litre, elapsed) == expected);
}

TEST_CASE("DispenserEmptyError names the dispenser and the pod", "[pod]")
{
    const cm::DispenserEmptyError error{cm::PodId{"Pod"}, cm::DispenserId{3}};
    CHECK(std::string{error.what()} == "Dispenser '3' of Pod 'Pod' is empty");
}

TEST_CASE("PodReceiveError copes with an error code it doesn't know", "[pod]")
{
    const cm::PodReceiveError error{cm::PodId{"Pod"}, static_cast<proto::field::ErrorCodeVal>(200)};
    CHECK(std::string_view{error.what()}.contains("Pod"));
}
