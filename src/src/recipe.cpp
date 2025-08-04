#include "cm/recipe.hpp"
#include <fmt/ranges.h>
#include "cm/commands/command.hpp"

namespace cm
{

RecipeBuilder::StepBuilder::StepBuilder::StepBuilder(RecipeBuilder &parent)
    : parent_{parent}
{}

Recipe::~Recipe() = default;

RecipeBuilder::StepBuilder &RecipeBuilder::StepBuilder::with_step(std::unique_ptr<Command> command)
{
    steps_.emplace_back(std::move(command));
    return *this;
}

RecipeBuilder &RecipeBuilder::StepBuilder::add()
{
    parent_.steps_.emplace_back(std::move(steps_));
    return parent_;
}

RecipeBuilder &RecipeBuilder::with_name(std::string name)
{
    name_ = std::move(name);
    return *this;
}

RecipeBuilder::StepBuilder RecipeBuilder::with_steps()
{
    return RecipeBuilder::StepBuilder{*this};
}

std::shared_ptr<Recipe> RecipeBuilder::create()
{
    return std::make_shared<Recipe>(std::move(name_), std::move(steps_));
}

RecipeBuilder make_recipe()
{
    return RecipeBuilder{};
}

Recipe::Recipe(std::string name, ProductionSteps steps)
    : name_{std::move(name)}
    , steps_{std::move(steps)}
{}

const ProductionSteps &Recipe::production_steps() const
{
    return steps_;
}

} // namespace cm

auto fmt::formatter<cm::Recipe>::format(const cm::Recipe &r, format_context &ctx) const -> format_context::iterator
{
    return formatter<string_view>::format(fmt::format("Recipe({})", r.name_), ctx);
}
