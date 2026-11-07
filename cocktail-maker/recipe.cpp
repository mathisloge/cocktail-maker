module;
#include <simdjson.h>

module cm;
import std;
import mp_units;
import cm.core;
import :catalog_loader;

namespace cm {
void assign_unique_command_ids(Recipe& recipe)
{
    CommandId next_id{1};
    for_each_command(recipe.commands, [&next_id](Command& command) {
        std::visit(
            [&next_id](auto& alternative) {
                if constexpr (requires { alternative.id; }) {
                    alternative.id = next_id++;
                }
            },
            command);
    });
}

void RecipeStore::init_recipes(std::vector<Recipe> recipes)
{
    std::unordered_set<RecipeId> ids;
    for (auto& recipe : recipes) {
        if (not ids.insert(recipe.id).second) {
            throw std::invalid_argument{std::format("Duplicate recipe id '{}'", recipe.id)};
        }
        assign_unique_command_ids(recipe);
    }
    recipes_ = std::move(recipes);
}

std::optional<Recipe> RecipeStore::find_by_id(RecipeId id) const
{
    auto it = std::ranges::find(recipes_, id, &Recipe::id);
    if (it == recipes_.end()) {
        return std::nullopt;
    }
    return *it;
}

std::optional<Recipe> RecipeStore::find_by_index(int index) const
{
    if (recipe_count() <= index or index < 0) {
        return std::nullopt;
    }
    return recipes_.at(index);
}

std::size_t RecipeStore::recipe_count() const
{
    return recipes_.size();
}

/// The command ids are left to `RecipeStore::init_recipes`, which numbers the commands of every recipe.
DispenseCommand parse_dispense_step(simdjson::ondemand::object& step, const IngredientStore& ingredient_store)
{
    DispenseCommand command;
    command.ingredient = IngredientId{std::string(step["ingredient"].get_string().value())};
    if (!ingredient_store.find_by_id(command.ingredient).has_value()) {
        throw std::runtime_error(std::format("Ingredient '{}' not found in store", command.ingredient));
    }

    command.volume = step["volume"].get_double().value() * units::milli_litre;
    if (command.volume < 1 * units::milli_litre) {
        throw std::runtime_error(std::format("The volume of '{}' has to be at least 1 mL", command.ingredient));
    }
    return command;
}

Recipe parse_recipe(simdjson::ondemand::document& doc,
                    const std::filesystem::path& file_path,
                    const IngredientStore& ingredient_store)
{
    Recipe recipe;
    simdjson::ondemand::object obj = doc.get_object();

    recipe.id = RecipeId{file_path.stem()};

    // Name (Required)
    recipe.display_name = obj["name"].get_string().value();

    // Description (Optional)
    std::string_view description_sv;
    if (obj["description"].get(description_sv) == simdjson::SUCCESS) {
        recipe.description = std::string(description_sv);
    }

    // Tags (Optional)
    simdjson::ondemand::array tags_arr;
    if (obj["tags"].get(tags_arr) == simdjson::SUCCESS) {
        for (auto tag_val : tags_arr) {
            recipe.tags.push_back(std::string(tag_val.get_string().value()));
        }
    }

    // Image Path (Same folder, .png extension instead of .json)
    std::filesystem::path img_path = file_path;
    img_path.replace_extension(".png");
    recipe.image_path = img_path;

    // Scaling divides by it, so it has to be there and above 0.
    double nominal_vol_ml = 0.0;
    if (obj["nominal_serving_volume"].get(nominal_vol_ml) != simdjson::SUCCESS || nominal_vol_ml <= 0.0) {
        throw std::runtime_error("nominal_serving_volume has to be a number above 0");
    }
    recipe.nominal_serving_volume = nominal_vol_ml * units::milli_litre;

    // Steps (Required array)
    simdjson::ondemand::array steps_arr = obj["steps"].get_array().value();

    for (auto step_val : steps_arr) {
        simdjson::ondemand::object step_obj = step_val.get_object().value();
        std::string_view kind = step_obj["kind"].get_string().value();

        if (kind == "dispense") {
            recipe.commands.emplace_back(Command{parse_dispense_step(step_obj, ingredient_store)});
        }
        else if (kind == "manual") {
            ManualCommand cmd;
            cmd.instruction = std::string(step_obj["text"].get_string().value());
            recipe.commands.emplace_back(Command{cmd});
        }
        else if (kind == "parallel") {
            ParallelCommand pcmd;
            simdjson::ondemand::array parallel_steps = step_obj["steps"].get_array().value();

            for (auto inner_val : parallel_steps) {
                simdjson::ondemand::object inner_obj = inner_val.get_object().value();
                std::string_view inner_kind = inner_obj["kind"].get_string().value();

                if (inner_kind != "dispense") {
                    throw std::runtime_error("Only dispense steps are allowed inside a parallel block");
                }
                pcmd.emplace_back(Command{parse_dispense_step(inner_obj, ingredient_store)});
            }
            recipe.commands.emplace_back(pcmd);
        }
        else {
            throw std::runtime_error(std::format("Unknown recipe step kind: '{}'", kind));
        }
    }

    return recipe;
}

std::vector<Recipe> load_recipes_from_dir(const std::filesystem::path& path, const IngredientStore& ingredient_store)
{
    return load_json_catalog<Recipe>(
        path, "recipe", [&ingredient_store](simdjson::ondemand::document& document, const std::filesystem::path& file) {
            return parse_recipe(document, file, ingredient_store);
        });
}
} // namespace cm
