// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/recipe.hpp"
#include <fmt/ranges.h>
#include <libassert/assert.hpp>
#include "cm/commands/command.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"

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

RecipeBuilder& RecipeBuilder::with_nominal_serving_volume(units::Litre litre)
{
    nominal_serving_volume_ = litre;
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
    ASSERT(not name_.empty());
    ASSERT(nominal_serving_volume_ > 0 * units::si::litre);
    return std::make_shared<Recipe>(
        std::move(name_), std::move(steps_), std::move(description_), std::move(image_path_), nominal_serving_volume_);
}

RecipeBuilder make_recipe()
{
    return RecipeBuilder{};
}

Recipe::Recipe(std::string name,
               ProductionSteps steps,
               std::string description,
               std::filesystem::path image_path,
               units::Litre nominal_serving_volume)
    : name_{std::move(name)}
    , description_{std::move(description)}
    , image_path_{std::move(image_path)}
    , steps_{std::move(steps)}
    , nominal_serving_volume_{nominal_serving_volume}
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

units::Litre Recipe::nominal_serving_volume() const
{
    return nominal_serving_volume_;
}

std::shared_ptr<Recipe> Recipe::scaled_to(const units::Litre target_volume) const
{
    using units::si::litre;

    // protect against degenerate nominal
    if (target_volume <= 0.0 * litre) {
        throw std::out_of_range("can't scale recipe into negative");
    }

    // compute scale factor (dimensionless)
    const auto scale = target_volume / nominal_serving_volume_;

    // visitor that clones commands, scaling dispense volumes
    struct Scaler : public CommandVisitor
    {
        explicit Scaler(units::quantity<units::one> s)
            : scale(s)
        {
        }

        void visit(const DispenseLiquidCmd& cmd) override
        {
            auto new_vol = cmd.volume() * scale;
            if (new_vol < 0.0 * units::si::litre) {
                new_vol = 0.0 * units::si::litre;
            }
            cloned = std::make_unique<DispenseLiquidCmd>(cmd.ingredient(), new_vol, cmd.id());
        }

        void visit(const ManualCmd& cmd) override
        {
            cloned = std::make_unique<ManualCmd>(cmd.instruction(), cmd.id());
        }

        std::unique_ptr<Command> take()
        {
            return std::move(cloned);
        }

        units::quantity<units::one> scale;
        std::unique_ptr<Command> cloned;
    };

    ProductionSteps new_steps;
    new_steps.reserve(steps_.size());

    for (const auto& step : steps_) {
        ProductionStep new_step;
        new_step.reserve(step.size());

        for (const auto& cmd : step) {
            Scaler s{scale};
            cmd->accept(s);
            new_step.emplace_back(s.take());
        }

        new_steps.emplace_back(std::move(new_step));
    }

    return std::make_shared<Recipe>(name_, std::move(new_steps), description_, image_path_, target_volume);
}

} // namespace cm

auto fmt::formatter<cm::Recipe>::format(const cm::Recipe& r, format_context& ctx) const -> format_context::iterator
{
    return formatter<string_view>::format(fmt::format("Recipe({})", r.name_), ctx);
}
