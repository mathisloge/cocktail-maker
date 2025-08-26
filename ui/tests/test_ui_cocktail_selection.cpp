// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include <quite/test/probe.hpp>
#include <quite/test/probe_manager.hpp>
#include <quite/value/object_query.hpp>
#include "application_path.hpp"

using quite::make_query;
using namespace std::chrono_literals;

constexpr auto kObjectName = "objectName";

class AppFixture
{
  public:
    AppFixture()
        : probe{manager.launch_probe_application("cocktail-maker", kApplicationPath)}
    {
        probe.wait_for_connected(10s);
    }

    void wait_for_stack_animation()
    {
        auto stack_view = probe.find_object(make_query().with_property(kObjectName, std::string{"stackView"}));
        auto busy_prop = stack_view.property("busy");
        REQUIRE(std::get<bool>(busy_prop.wait_for_value(true, 500ms)));
        REQUIRE_FALSE(std::get<bool>(busy_prop.wait_for_value(false, 2s)));
    }

  protected:
    quite::test::ProbeManager manager;
    quite::test::Probe probe;
};

TEST_CASE_METHOD(AppFixture, "Select a cocktail", "[ui]")
{
    GIVEN("The Mojito recipe is selected")
    {
        auto mojito = probe.find_object(make_query().with_property(kObjectName, std::string{"recipe-Mojito"}));
        mojito.mouse_action();
        wait_for_stack_animation();
        REQUIRE_NOTHROW(probe.try_find_object(make_query().with_property(kObjectName, std::string{"recipeDetailPage"}), 1s));

        WHEN("I look at the ingredient list")
        {
            THEN("The ingredients are properly listed")
            {
                REQUIRE_NOTHROW(probe.find_object(make_query().with_property("text", std::string{"Mojito"})));
                // list
                auto repeater = probe.find_object(make_query().with_property(kObjectName, std::string{"ingredientRepeater"}));
                REQUIRE(std::get<std::int64_t>(repeater.property("count").value()) == 5);

                const std::array<std::string, 5> expected_ingredients{
                    "Bacardi", "Soda Wasser", "Limettensaft", "Manuell", "Manuell"}; // codespell:ignore
                for (int i = 0; i < expected_ingredients.size(); i++) {
                    auto item = std::get<quite::test::RemoteObject>(repeater.invoke_method("itemAt(int)", {i}));
                    REQUIRE(std::get<std::string>(item.property("name").value()) == expected_ingredients.at(i));
                }
            }
        }
    }
}
