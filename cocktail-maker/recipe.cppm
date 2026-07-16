module;

export module cm:recipe;
import std;
import cm.core;
import :ingredient;

namespace cm {

export using RecipeId = strong_type<std::string, struct CommandIdTag, Comparable, Hashable, Formattable>;
export using CommandId = strong_type<int, struct CommandIdTag, Incrementable, Decrementable, Comparable, Hashable, Formattable>;

export struct DispenseCommand
{
    CommandId id;
    IngredientId ingredient;
    units::Litre volume;
};

export struct ManualCommand
{
    CommandId id;
    std::string instruction;
};

export using Command = std::variant<std::monostate, ManualCommand, DispenseCommand>;
export using ParallelCommand = std::vector<Command>;
export using Commands = std::vector<std::variant<Command, ParallelCommand>>;

export struct Recipe
{
    RecipeId id;
    std::string display_name;
    std::string description;
    std::vector<std::string> tags;
    std::filesystem::path image_path;
    // The sum of the dispensing commands can be less then the nominal_serving_volume (e.g. because of ice cubes displacement
    // etc.)
    units::Litre nominal_serving_volume;
    Commands commands;

    friend struct std::formatter<Recipe>;
};

std::vector<Recipe> load_recipes_from_dir(const std::filesystem::path& path, const IngredientStore& ingredient_store);

export class RecipeStore final
{
  public:
    void init_recipes(std::vector<Recipe> recipes);

    std::optional<Recipe> find_by_id(RecipeId id) const;

    std::optional<Recipe> find_by_index(int index) const;

    std::size_t recipe_count() const;

  private:
    std::vector<Recipe> recipes_;
};
} // namespace cm

export template <>
struct std::formatter<cm::Recipe> : formatter<string_view>
{
    auto format(const cm::Recipe& r, format_context& ctx) const -> format_context::iterator
    {
        return formatter<string_view>::format(std::format("Recipe(id={}, name={})", r.id, r.display_name), ctx);
    }
};
