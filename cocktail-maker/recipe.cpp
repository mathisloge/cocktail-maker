module;
#include <simdjson/builder.h>
#include <simdjson/ondemand.h>
#include <simdjson/padded_string.h>

module cm:recipe_impl;
import std;
import mp_units;
import cm.core;
import :recipe;

namespace cm {
template <typename T>
struct is_variant : std::false_type
{
};

template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_variant_v = is_variant<std::remove_cvref_t<T>>::value;

constexpr void assign_unique_command_ids(Recipe& recipe)
{
    CommandId next_id{1};
    auto visitor = [&next_id](this auto const& self, auto& node) -> void {
        // Does this node have a command_id? Assign Id.
        if constexpr (requires { node.id; }) {
            node.id = next_id++;
        }
        // Matches Command and the top-level variants in Commands.
        else if constexpr (is_variant_v<decltype(node)>) {
            std::visit(self, node);
        }
        // Matches Commands and ParallelCommand (which are std::vector under the hood).
        else if constexpr (std::ranges::range<decltype(node)>) {
            for (auto& child : node) {
                self(child); // Recursively process children
            }
        }
    };
    visitor(recipe.commands);
}


void RecipeStore::init_recipes(std::vector<Recipe> recipes)
{
    for (auto&& r : recipes) {
        assign_unique_command_ids(r);
        recipes_.emplace_back(std::move(r));
    }
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

    // Nominal Serving Volume (Optional, defaults to 0.0 Litres)
    double nominal_vol_ml = 0.0;
    if (obj["nominal_serving_volume"].get(nominal_vol_ml) == simdjson::SUCCESS) {
        recipe.nominal_serving_volume = nominal_vol_ml * units::milli_litre;
    }
    else {
        recipe.nominal_serving_volume = nominal_vol_ml * units::milli_litre;
    }

    // Steps (Required array)
    int next_command_id = 1;
    simdjson::ondemand::array steps_arr = obj["steps"].get_array().value();

    for (auto step_val : steps_arr) {
        simdjson::ondemand::object step_obj = step_val.get_object().value();
        std::string_view kind = step_obj["kind"].get_string().value();

        if (kind == "dispense") {
            DispenseCommand cmd;
            cmd.id = CommandId{next_command_id++};
            cmd.ingredient = IngredientId{std::string(step_obj["ingredient"].get_string().value())};

            // Validate against the provided ingredient store
            if (!ingredient_store.find_by_id(cmd.ingredient).has_value()) {
                throw std::runtime_error(
                    std::format("Ingredient '{}' not found in store", static_cast<const std::string&>(cmd.ingredient)));
            }

            cmd.volume = step_obj["volume"].get_double().value() * units::milli_litre;
            recipe.commands.emplace_back(Command{cmd});
        }
        else if (kind == "manual") {
            ManualCommand cmd;
            cmd.id = CommandId{next_command_id++};
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

                DispenseCommand cmd;
                cmd.id = CommandId{next_command_id++};
                cmd.ingredient = IngredientId{std::string(inner_obj["ingredient"].get_string().value())};

                if (!ingredient_store.find_by_id(cmd.ingredient).has_value()) {
                    throw std::runtime_error(
                        std::format("Ingredient '{}' not found in store", static_cast<const std::string&>(cmd.ingredient)));
                }

                cmd.volume = inner_obj["volume"].get_double().value() * units::milli_litre;
                pcmd.emplace_back(Command{cmd});
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
    std::vector<Recipe> recipes;
    simdjson::ondemand::parser parser;

    auto load_single_file = [&](const std::filesystem::path& file_path) {
        try {
            simdjson::padded_string json = simdjson::padded_string::load(file_path.string());
            simdjson::ondemand::document doc = parser.iterate(json);
            recipes.push_back(parse_recipe(doc, file_path, ingredient_store));
        }
        catch (const simdjson::simdjson_error& e) {
            throw std::runtime_error(std::format("Failed to parse recipe {}: {}", file_path.string(), e.what()));
        }
        catch (const std::exception& e) {
            throw std::runtime_error(std::format("Error loading recipe {}: {}", file_path.string(), e.what()));
        }
    };

    if (std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                load_single_file(entry.path());
            }
        }
    }
    else if (std::filesystem::is_regular_file(path) && path.extension() == ".json") {
        load_single_file(path);
    }

    return recipes;
}
} // namespace cm