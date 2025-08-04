#pragma once
#include <memory>
#include <vector>
#include <fmt/core.h>

namespace cm
{
class Command;
class Recipe;

using ProductionStep = std::vector<std::unique_ptr<Command>>;
using ProductionSteps = std::vector<ProductionStep>;
class RecipeBuilder
{
  public:
    class StepBuilder
    {
        RecipeBuilder &parent_;
        ProductionStep steps_;

      public:
        StepBuilder(RecipeBuilder &parent);
        StepBuilder &with_step(std::unique_ptr<Command> command);
        RecipeBuilder &add();
    };
    RecipeBuilder &with_name(std::string name);
    StepBuilder with_steps();
    std::shared_ptr<Recipe> create();

  private:
    std::string name_;
    ProductionSteps steps_;
};
RecipeBuilder make_recipe();

class Recipe
{
  public:
    Recipe(std::string name, ProductionSteps steps);
    ~Recipe();

    const ProductionSteps &production_steps() const;

  private:
    std::string name_;
    ProductionSteps steps_;

    friend struct fmt::formatter<Recipe>;
};
} // namespace cm

template <>
struct fmt::formatter<cm::Recipe> : formatter<string_view>
{
    auto format(const cm::Recipe &r, format_context &ctx) const -> format_context::iterator;
};
