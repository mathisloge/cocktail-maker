module;

export module cm:recipe;
import std;
import :ingredient;
import :units;

namespace cm {

export using CommandId = int;

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
    int id;
    std::string display_name;
    std::string description;
    std::vector<std::string> tags;
    std::filesystem::path image_path;
    Commands commands;

    friend struct std::formatter<Recipe>;
};

// TODO: implement simdjson loading with reflection
std::vector<Recipe> load_from_disk(const std::filesystem::path& path, const IngredientStore& ingredient_store);

namespace {
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
    int next_id = 1;
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

} // namespace

export class RecipeStore
{
  public:
    explicit RecipeStore(std::vector<Recipe> recipes)
    {
        int id{0};
        for (auto&& r : recipes) {
            r.id = id++;
            assign_unique_command_ids(r);
            recipes_.emplace_back(std::move(r));
        }
    }

    std::optional<Recipe> find_by_id(int id) const
    {
        return find_by_index(id); // inserted in order of id.
    }

    std::optional<Recipe> find_by_index(int index) const
    {
        if (recipe_count() <= index or index < 0) {
            return std::nullopt;
        }
        return recipes_.at(index);
    }

    std::size_t recipe_count() const
    {
        return recipes_.size();
    }

  private:
    std::vector<Recipe> recipes_;
};
} // namespace cm

export template <>
struct std::formatter<cm::Recipe> : formatter<string_view>
{
    auto format(const cm::Recipe& r, format_context& ctx) const -> format_context::iterator
    {
        return formatter<string_view>::format(std::format("Recipe(name={})", r.display_name), ctx);
    }
};
