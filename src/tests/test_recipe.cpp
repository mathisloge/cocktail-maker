// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <cm/recipe.hpp>
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"

using namespace cm;

namespace {
consteval auto L(double litres) // NOLINT
{
    return litres * units::si::litre;
}
} // namespace

TEST_CASE("scaled_to preserves structure and scales dispense volumes", "[recipe][scaling]")
{
    // build recipe with two dispense commands in one step
    auto builder = make_recipe().with_name("ScaleTest").with_nominal_serving_volume(L(0.25)).with_steps();

    builder.with_step(std::make_unique<DispenseLiquidCmd>("Vodka", L(0.04), generate_unique_command_id()));
    builder.with_step(std::make_unique<DispenseLiquidCmd>("Cola", L(0.10), generate_unique_command_id()));
    auto recipe = builder.add().create();

    REQUIRE(recipe->production_steps().size() == 1);
    REQUIRE(recipe->production_steps()[0].size() == 2);

    auto&& orig_step = recipe->production_steps()[0];
    const auto* d0 = dynamic_cast<const DispenseLiquidCmd*>(orig_step[0].get());
    const auto* d1 = dynamic_cast<const DispenseLiquidCmd*>(orig_step[1].get());
    REQUIRE(d0 != nullptr);
    REQUIRE(d1 != nullptr);

    const auto v0 = d0->volume();
    const auto v1 = d1->volume();

    // scale down to 200ml (0.2 L)
    const auto target = L(0.20);
    auto scaled = recipe->scaled_to(target);

    REQUIRE(scaled->nominal_serving_volume() == target);
    REQUIRE(scaled->production_steps().size() == recipe->production_steps().size());
    REQUIRE(scaled->production_steps()[0].size() == recipe->production_steps()[0].size());

    const auto* sd0 = dynamic_cast<const DispenseLiquidCmd*>(scaled->production_steps()[0][0].get());
    const auto* sd1 = dynamic_cast<const DispenseLiquidCmd*>(scaled->production_steps()[0][1].get());
    REQUIRE(sd0 != nullptr);
    REQUIRE(sd1 != nullptr);

    // expected scale factor = target / original nominal
    const auto expected_scale = target / recipe->nominal_serving_volume();
    REQUIRE(sd0->volume() == v0 * expected_scale);
    REQUIRE(sd1->volume() == v1 * expected_scale);
}

TEST_CASE("scaled_to rejects non-positive target volume", "[recipe][scaling]")
{
    auto recipe = make_recipe()
                      .with_name("BadScale")
                      .with_nominal_serving_volume(L(0.25))
                      .with_steps()
                      .with_step(std::make_unique<ManualCmd>("noop", generate_unique_command_id()))
                      .add()
                      .create();

    REQUIRE_THROWS_AS(recipe->scaled_to(L(0.0)), std::out_of_range);
}
