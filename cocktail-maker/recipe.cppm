module;

export module cm:recipe;
import std;
import fmt;
import :ingredient;
import :units;

namespace cm {
export struct DispenseCommand
{
    IngredientId ingredient;
    units::Litre volume;
};

export struct ManualCommand
{
    std::string instruction;
};

export using Command = std::variant<std::monostate, ManualCommand, DispenseCommand>;
export using Commands = std::vector<Command>;

export struct Recipe
{
    std::string display_name;
    std::string description;
    std::vector<std::string> tags;
    std::filesystem::path image_path;
    Commands commands;

    friend struct fmt::formatter<Recipe>;
};

// TODO: implement simdjson loading with reflection
std::vector<Recipe> load_from_disk(const std::filesystem::path& path, const IngredientStore& ingredient_store);

} // namespace cm

export template <>
struct fmt::formatter<cm::Recipe> : formatter<string_view>
{
    auto format(const cm::Recipe& r, format_context& ctx) const -> format_context::iterator
    {
        return formatter<string_view>::format(fmt::format("Recipe(name={})", r.display_name), ctx);
    }
};
