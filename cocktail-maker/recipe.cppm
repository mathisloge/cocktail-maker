module;

export module cm:recipe;
import std;
import cm.core;
import :ingredient;

namespace cm {

export using RecipeId = strong_type<std::string, struct RecipeIdTag, Comparable, Hashable, Formattable>;
export using CommandId = strong_type<int, struct CommandIdTag, Incrementable, Comparable, Hashable, Formattable>;

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

/// Calls `fn` with every command of `commands` in order, the ones inside a parallel group included.
export template <typename CommandList, typename Fn>
    requires std::same_as<std::remove_cvref_t<CommandList>, Commands>
void for_each_command(CommandList&& commands, Fn&& fn)
{
    for (auto&& step : commands) {
        if (auto* command = std::get_if<Command>(&step)) {
            fn(*command);
            continue;
        }
        for (auto&& member : std::get<ParallelCommand>(step)) {
            fn(member);
        }
    }
}

/// A copy of `commands` with every command replaced by `fn(command)`. A parallel group stays a group.
export template <typename Fn>
Commands transform_commands(const Commands& commands, Fn&& fn)
{
    Commands result;
    result.reserve(commands.size());
    for (const auto& step : commands) {
        if (const auto* command = std::get_if<Command>(&step)) {
            result.emplace_back(std::in_place_type<Command>, fn(*command));
            continue;
        }
        const auto& members = std::get<ParallelCommand>(step);
        ParallelCommand group;
        group.reserve(members.size());
        for (const auto& member : members) {
            group.push_back(fn(member));
        }
        result.emplace_back(std::move(group));
    }
    return result;
}

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

/**
 * Loads every recipe file in the directory `path`, in the order of the file names.
 *
 * @throws std::runtime_error naming the file, if a recipe can't be read, names an unknown ingredient, or has a value
 * the Station can't work with: a serving volume of 0 or less, or a dispense step below 1 mL.
 * @throws std::filesystem::filesystem_error if `path` isn't a directory.
 */
export std::vector<Recipe> load_recipes_from_dir(const std::filesystem::path& path, const IngredientStore& ingredient_store);

export class RecipeStore final
{
  public:
    /// Replaces the recipes and numbers their commands. @throws std::invalid_argument if two recipes share an id.
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
