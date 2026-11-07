#include <catch2/catch_test_macros.hpp>

import std;
import cm.core;
import cm.testing;
import cm;
import cm.gui;

using cm::gui::ErrorItemCategory;

namespace {
/// A catalog that knows rum, which is assigned to dispenser 0 of Pod "Pod".
struct MixErrorFixture
{
    cm::testing::TempDir dir{"cm-mix-error-page-test"};
    cm::IngredientStore ingredients;
    cm::StationConfig config{ingredients, dir.file("mapping.json")};

    MixErrorFixture()
    {
        ingredients.init_ingredients({cm::Ingredient{.id = cm::IngredientId{"rum"}, .display_name = "Rum", .type = {}}});
        config.update_dispenser_ingredient_mapping(cm::IngredientId{"rum"}, {cm::PodId{"Pod"}, cm::DispenserId{0}});
    }

    template <typename Error>
    cm::gui::MixErrorPage page(Error error) const
    {
        return cm::gui::mix_error_page(std::make_exception_ptr(std::move(error)), config, ingredients);
    }
};
} // namespace

// codespell:ignore-begin
TEST_CASE_METHOD(MixErrorFixture, "An empty bottle names the ingredient", "[gui][mix_error_page]")
{
    const auto p = page(cm::DispenserEmptyError{cm::PodId{"Pod"}, cm::DispenserId{0}});

    CHECK(p.item_category == ErrorItemCategory::Ingredient);
    CHECK(p.item_name == "Rum");
    CHECK(p.customer_title == "Zutat nicht verfügbar");
}

TEST_CASE_METHOD(MixErrorFixture, "An empty bottle without an assignment still says what to refill", "[gui][mix_error_page]")
{
    const auto p = page(cm::DispenserEmptyError{cm::PodId{"Pod"}, cm::DispenserId{5}});

    CHECK(p.item_name == "Unbekannte Zutat");
    CHECK(p.item_details == "Bitte Behälter '5' von Pod 'Pod' befüllen");
}

TEST_CASE_METHOD(MixErrorFixture, "A Pod that is gone asks to check the connection", "[gui][mix_error_page]")
{
    const auto p = page(cm::PodDisconnectedError{cm::PodId{"Pod"}});

    CHECK(p.technical_title == "Pod nicht verbunden!");
    CHECK(p.item_details == "Pod 'Pod' - Bitte USB-Verbindung prüfen.");
}

TEST_CASE_METHOD(MixErrorFixture, "A Pod that doesn't answer asks to check the connection", "[gui][mix_error_page]")
{
    CHECK(page(cm::PodTimeoutError{cm::PodId{"Pod"}}).technical_title == "Pod antwortet nicht!");
}

TEST_CASE_METHOD(MixErrorFixture, "An abort is the guest's own doing", "[gui][mix_error_page]")
{
    const auto p = page(cm::gui::AbortError{});

    CHECK(p.item_category == ErrorItemCategory::Abort);
    CHECK(p.customer_title == "Abgebrochen");
}

TEST_CASE_METHOD(MixErrorFixture, "Any other error shows its details", "[gui][mix_error_page]")
{
    const auto p = page(std::runtime_error{"boom"});

    CHECK(p.technical_title == "Unbekannter Fehler!");
    CHECK(p.item_details.ends_with("boom"));
    CHECK(p.customer_title == "Kurze Unterbrechung");
}

// codespell:ignore-end
