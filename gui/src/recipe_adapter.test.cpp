#include <catch2/catch_test_macros.hpp>
#include <slint.h>
#include "app-window.h"

import std;
import cm;
import cm.gui;

using namespace cm::gui;

namespace {

cm::Recipe make_recipe(std::string name, std::vector<std::string> tags, std::string description = "")
{
    if (description.empty()) {
        description = "desc_" + name;
    }
    return cm::Recipe{
        .display_name = std::move(name),
        .description = std::move(description),
        .tags = std::move(tags),
        .image_path = "",
    };
}

// Helper: extract tag strings from a RecipeView's tag_line model
std::vector<std::string> get_tags(const RecipeView& view)
{
    std::vector<std::string> result;
    const auto& model = view.tag_line;
    for (size_t i = 0; i < model->row_count(); ++i) {
        auto entry = model->row_data(i);
        if (entry) {
            result.emplace_back(entry->data());
        }
    }
    return result;
}

} // namespace

TEST_CASE("RecipeModel general conversion from std::vector<Recipe>", "[recipe_adapter][conversion]")
{
    SECTION("empty input produces empty model")
    {
        RecipeModel model({});
        CHECK(model.row_count() == 0);
    }

    SECTION("row_count matches number of recipes")
    {
        RecipeModel model({
            make_recipe("A", {}),
            make_recipe("B", {}),
            make_recipe("C", {}),
        });
        CHECK(model.row_count() == 3);
    }

    SECTION("display_name is preserved as SharedString")
    {
        RecipeModel model({make_recipe("Margherita Pizza", {})});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(std::string(row->name.data()) == "Margherita Pizza");
    }

    SECTION("description is preserved as SharedString")
    {
        RecipeModel model({make_recipe("Margherita Pizza", {}, "Just a basic pizza.")});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(std::string(row->description.data()) == "Just a basic pizza.");
    }

    SECTION("out-of-bounds row_data returns nullopt")
    {
        RecipeModel model({make_recipe("Solo", {})});
        CHECK_FALSE(model.row_data(1).has_value());
        CHECK_FALSE(model.row_data(99).has_value());
    }

    SECTION("row_data(0) on empty model returns nullopt")
    {
        RecipeModel model({});
        CHECK_FALSE(model.row_data(0).has_value());
    }

    SECTION("each recipe maps to its own row in order")
    {
        RecipeModel model({
            make_recipe("First", {"a"}),
            make_recipe("Second", {"b"}),
            make_recipe("Third", {"c"}),
        });

        CHECK(std::string(model.row_data(0)->name.data()) == "First");
        CHECK(std::string(model.row_data(1)->name.data()) == "Second");
        CHECK(std::string(model.row_data(2)->name.data()) == "Third");
    }

    SECTION("tag_line row_count matches original tags size")
    {
        RecipeModel model({make_recipe("Taggy", {"x", "y", "z"})});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(row->tag_line->row_count() == 3);
    }

    SECTION("recipes with identical names are stored as distinct rows")
    {
        RecipeModel model({
            make_recipe("Clone", {"original"}),
            make_recipe("Clone", {"copy"}),
        });

        REQUIRE(model.row_count() == 2);
        CHECK(get_tags(*model.row_data(0))[0] == "ORIGINAL");
        CHECK(get_tags(*model.row_data(1))[0] == "COPY");
    }

    SECTION("image is loaded from path")
    {
        cm::Recipe r = make_recipe("Mojito", {});
        r.image_path = std::filesystem::path{TEST_IMAGE_DIR} / "mojito.png";

        RecipeModel model({std::move(r)});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());

        const auto& img = row->image;
        CHECK(img.size().width == 400);
        CHECK(img.size().height == 400);
    }

    SECTION("empty image_path produces a null/empty image")
    {
        RecipeModel model({make_recipe("No Image", {})});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());

        const auto& img = row->image;
        CHECK(img.size().width == 0);
        CHECK(img.size().height == 0);
    }
}

TEST_CASE("RecipeModel tag uppercasing", "[recipe_adapter][tags]")
{
    SECTION("lowercase tags are uppercased")
    {
        RecipeModel model({make_recipe("Pasta", {"vegan", "quick"})});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());

        auto tags = get_tags(*row);
        REQUIRE(tags.size() == 2);
        CHECK(tags[0] == "VEGAN");
        CHECK(tags[1] == "QUICK");
    }

    SECTION("already-uppercase tags are unchanged")
    {
        RecipeModel model({make_recipe("Steak", {"MEAT", "GRILL"})});

        auto tags = get_tags(*model.row_data(0));
        CHECK(tags[0] == "MEAT");
        CHECK(tags[1] == "GRILL");
    }

    SECTION("mixed-case tags are fully uppercased")
    {
        RecipeModel model({make_recipe("Soup", {"GluTen-Free", "SpIcY"})});

        auto tags = get_tags(*model.row_data(0));
        CHECK(tags[0] == "GLUTEN-FREE");
        CHECK(tags[1] == "SPICY");
    }

    SECTION("empty tag string remains empty")
    {
        RecipeModel model({make_recipe("Mystery", {""})});

        auto tags = get_tags(*model.row_data(0));
        REQUIRE(tags.size() == 1);
        CHECK(tags[0] == "");
    }

    SECTION("recipe with no tags has empty tag_line model")
    {
        RecipeModel model({make_recipe("Plain", {})});

        auto row = model.row_data(0);
        REQUIRE(row.has_value());
        CHECK(row->tag_line->row_count() == 0);
    }

    SECTION("tags are uppercased independently per recipe")
    {
        RecipeModel model({
            make_recipe("A", {"vegan"}),
            make_recipe("B", {"meat"}),
        });

        CHECK(get_tags(*model.row_data(0))[0] == "VEGAN");
        CHECK(get_tags(*model.row_data(1))[0] == "MEAT");
    }

    SECTION("numeric and symbol characters in tags are preserved")
    {
        RecipeModel model({make_recipe("Weird", {"top-5", "30min"})});

        auto tags = get_tags(*model.row_data(0));
        CHECK(tags[0] == "TOP-5");
        CHECK(tags[1] == "30MIN");
    }
}
