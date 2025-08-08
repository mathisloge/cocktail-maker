// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/recipe.hpp"
#include <fmt/ranges.h>
#include "cm/commands/command.hpp"

namespace cm {

RecipeBuilder::StepBuilder::StepBuilder::StepBuilder(RecipeBuilder& parent)
    : parent_{parent}
{
}

Recipe::~Recipe() = default;

RecipeBuilder::StepBuilder& RecipeBuilder::StepBuilder::with_step(std::unique_ptr<Command> command)
{
    steps_.emplace_back(std::move(command));
    return *this;
}

RecipeBuilder& RecipeBuilder::StepBuilder::add()
{
    parent_.steps_.emplace_back(std::move(steps_));
    return parent_;
}

RecipeBuilder& RecipeBuilder::with_name(std::string name)
{
    name_ = std::move(name);
    return *this;
}

RecipeBuilder& RecipeBuilder::with_description(std::string description)
{
    description_ = std::move(description);
    return *this;
}

RecipeBuilder& RecipeBuilder::with_image(std::filesystem::path image_path)
{
    image_path_ = std::move(image_path);
    return *this;
}

RecipeBuilder::StepBuilder RecipeBuilder::with_steps()
{
    return RecipeBuilder::StepBuilder{*this};
}

std::shared_ptr<Recipe> RecipeBuilder::create()
{
    return std::make_shared<Recipe>(
        std::move(name_), std::move(steps_), std::move(description_), std::move(image_path_));
}

RecipeBuilder make_recipe()
{
    return RecipeBuilder{};
}

Recipe::Recipe(std::string name,
               ProductionSteps steps,
               std::string description,
               std::filesystem::path image_path)
    : name_{std::move(name)}
    , description_{std::move(description)}
    , image_path_{std::move(image_path)}
    , steps_{std::move(steps)}
{
}

const std::string& Recipe::name() const
{
    return name_;
}

const std::string& Recipe::description() const
{
    return description_;
}

const std::filesystem::path& Recipe::image_path() const
{
    return image_path_;
}

const ProductionSteps& Recipe::production_steps() const
{
    return steps_;
}

} // namespace cm

auto fmt::formatter<cm::Recipe>::format(const cm::Recipe& r, format_context& ctx) const
    -> format_context::iterator
{
    return formatter<string_view>::format(fmt::format("Recipe({})", r.name_), ctx);
}
