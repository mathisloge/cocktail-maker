#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
import std;
import mp_units;
import cm;
import cm.core;

namespace {

using namespace cm;
using Catch::Matchers::WithinAbs;

[[nodiscard]] auto dispense(int id, std::string ingredient, double volume_ml) -> Command
{
    return DispenseCommand{CommandId{id}, IngredientId{std::move(ingredient)}, volume_ml * units::milli_litre};
}

[[nodiscard]] auto manual(int id, std::string instruction) -> Command
{
    return ManualCommand{CommandId{id}, std::move(instruction)};
}

// Extracts the dispensed volume in ml, or 0.0 for non-dispense commands.
[[nodiscard]] auto volume_ml(const Command& cmd) -> double
{
    return std::visit(
        [](const auto& c) -> double {
            if constexpr (std::is_same_v<std::decay_t<decltype(c)>, DispenseCommand>) {
                return c.volume.numerical_value_in(units::milli_litre);
            }
            else {
                return 0.0;
            }
        },
        cmd);
}

} // namespace

TEST_CASE("scale_recipe scales DispenseCommand volumes proportionally", "[recipe_scale]")
{
    const Commands commands = {dispense(1, "gin", 50.0), dispense(2, "tonic", 150.0)};

    SECTION("scaling up")
    {
        const auto scaled = cm::scale_recipe(commands, 200.0 * units::milli_litre, 400.0 * units::milli_litre);

        REQUIRE(scaled.size() == 2);
        CHECK_THAT(volume_ml(std::get<Command>(scaled[0])), WithinAbs(100.0, 1e-9));
        CHECK_THAT(volume_ml(std::get<Command>(scaled[1])), WithinAbs(300.0, 1e-9));
    }

    SECTION("scaling down")
    {
        const auto scaled = cm::scale_recipe(commands, 200.0 * units::milli_litre, 100.0 * units::milli_litre);

        REQUIRE(scaled.size() == 2);
        CHECK_THAT(volume_ml(std::get<Command>(scaled[0])), WithinAbs(25.0, 1e-9));
        CHECK_THAT(volume_ml(std::get<Command>(scaled[1])), WithinAbs(75.0, 1e-9));
    }
}

TEST_CASE("scale_recipe leaves ManualCommand entries untouched", "[recipe_scale]")
{
    const Commands commands = {dispense(1, "gin", 50.0), manual(2, "Stir gently")};

    const auto scaled = cm::scale_recipe(commands, 50.0 * units::milli_litre, 500.0 * units::milli_litre);

    REQUIRE(scaled.size() == 2);
    CHECK_THAT(volume_ml(std::get<Command>(scaled[0])), WithinAbs(500.0, 1e-9));
    REQUIRE(std::holds_alternative<ManualCommand>(std::get<Command>(scaled[1])));
    CHECK(std::get<ManualCommand>(std::get<Command>(scaled[1])).instruction == "Stir gently");
}

TEST_CASE("scale_recipe scales every command inside a ParallelCommand", "[recipe_scale]")
{
    const ParallelCommand parallel = {dispense(1, "gin", 30.0), dispense(2, "vermouth", 10.0)};
    const Commands commands = {parallel};

    const auto scaled = cm::scale_recipe(commands, 40.0 * units::milli_litre, 80.0 * units::milli_litre);

    REQUIRE(scaled.size() == 1);
    const auto& scaled_parallel = std::get<ParallelCommand>(scaled[0]);
    REQUIRE(scaled_parallel.size() == 2);
    CHECK_THAT(volume_ml(scaled_parallel[0]), WithinAbs(60.0, 1e-9));
    CHECK_THAT(volume_ml(scaled_parallel[1]), WithinAbs(20.0, 1e-9));
}

TEST_CASE("scale_recipe is a no-op when target equals nominal volume", "[recipe_scale]")
{
    const Commands commands = {dispense(1, "gin", 50.0)};

    const auto scaled = cm::scale_recipe(commands, 200.0 * units::milli_litre, 200.0 * units::milli_litre);

    REQUIRE(scaled.size() == 1);
    CHECK_THAT(volume_ml(std::get<Command>(scaled[0])), WithinAbs(50.0, 1e-9));
}

TEST_CASE("scale_recipe's early-return actually triggers despite floating point noise", "[recipe_scale]")
{
    // Constructed so target_ml / nominal_ml is mathematically 1.0 but may
    // not land on the exact same bit pattern as 1.0 after division — this
    // is the case the epsilon comparison (rather than `factor == 1.0`) is
    // meant to catch. Ten steps of 1/3 mL accumulate rounding error that a
    // strict `== 1.0` would not absorb.
    constexpr auto kThird = (1.0 / 3.0) * units::milli_litre;
    constexpr auto kNominal = kThird * 10.0;

    const Commands commands = {dispense(1, "gin", 50.0)};
    const auto scaled = cm::scale_recipe(commands, kNominal, kNominal);

    // If the early return fired, the result is the exact same DispenseCommand
    // volume, not a "close enough" multiply. (At least try to detect early return.)
    REQUIRE(scaled.size() == 1);
    CHECK(volume_ml(std::get<Command>(scaled[0])) == 50.0);
}

TEST_CASE("scale_recipe is linear in target_volume across a range of factors", "[recipe_scale]")
{
    const auto factor = GENERATE(0.1, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 10.0);
    const Commands commands = {dispense(1, "gin", 40.0)};
    constexpr auto kNominalMl = 200.0 * units::milli_litre;

    const auto scaled = cm::scale_recipe(commands, kNominalMl, kNominalMl * factor);

    REQUIRE(scaled.size() == 1);
    CHECK_THAT(volume_ml(std::get<Command>(scaled[0])), WithinAbs(40.0 * factor, 1e-9));
}
