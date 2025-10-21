// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/recipe_booster.hpp"
#include <mp-units/math.h>
#include <mp-units/systems/si/unit_symbols.h>
#include "cm/commands/command_visitor.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"
#include "cm/ingredient_store.hpp"
#include "cm/recipe.hpp"

namespace cm {
using units::si::litre;

namespace {
class TotalsVisitor : public CommandVisitor
{
  public:
    explicit TotalsVisitor(const IngredientStore& ingredient_store)
        : ingredient_store_{ingredient_store}
    {
    }

    void visit(const DispenseLiquidCmd& cmd) override
    {
        const auto& ingredient = ingredient_store_.find_ingredient(cmd.ingredient());
        const auto vol = cmd.volume();

        switch (ingredient.boost_category) {
        case BoostCategory::boostable:
            boostable_ += vol;
            break;
        case BoostCategory::reducible:
            reducible_ += vol;
            break;
        case BoostCategory::fixed:
            fixed_ += vol;
            break;
        }
    }

    void visit([[maybe_unused]] const ManualCmd& cmd) override
    {
        // keine Volumen-Änderung
    }

    units::Litre boostable() const
    {
        return boostable_;
    }

    units::Litre reducible() const
    {
        return reducible_;
    }

    units::Litre fixed() const
    {
        return fixed_;
    }

  private:
    const IngredientStore& ingredient_store_;
    units::Litre boostable_{};
    units::Litre reducible_{};
    units::Litre fixed_{};
};

class ApplyVisitor : public CommandVisitor
{
  public:
    ApplyVisitor(const IngredientStore& store,
                 units::quantity<units::one> scale_boostable,
                 units::quantity<units::one> scale_reducible)
        : store_{store}
        , scale_boostable_{scale_boostable}
        , scale_reducible_{scale_reducible}
    {
    }

    std::unique_ptr<Command> take_command()
    {
        return std::move(cloned_);
    }

    void visit(const DispenseLiquidCmd& cmd) override
    {
        const auto& info = store_.find_ingredient(cmd.ingredient());
        units::Litre new_volume = cmd.volume();

        switch (info.boost_category) {
        case BoostCategory::boostable:
            new_volume = cmd.volume() * scale_boostable_;
            break;
        case BoostCategory::reducible:
            new_volume = cmd.volume() * scale_reducible_;
            break;
        case BoostCategory::fixed:
            // Volume remains unchanged
            break;
        }

        // Prevent negative volumes due to numerical precision issues
        new_volume = std::max(new_volume, 0. * litre);

        cloned_ = std::make_unique<DispenseLiquidCmd>(cmd.ingredient(), new_volume, cmd.id());
    }

    void visit(const ManualCmd& cmd) override
    {
        cloned_ = std::make_unique<ManualCmd>(cmd.instruction(), cmd.id());
    }

  private:
    const IngredientStore& store_;
    std::unique_ptr<Command> cloned_;
    units::quantity<units::one> scale_boostable_{1.0};
    units::quantity<units::one> scale_reducible_{1.0};
};

constexpr double kPercentToFraction = 0.01;
constexpr units::Litre kZeroVolume = 0.0 * litre;

bool is_effectively_zero(units::Litre volume)
{
    return volume <= std::numeric_limits<double>::epsilon() * litre;
}

units::quantity<units::one> calculate_scale_factor(units::Litre original, units::Litre target)
{
    return is_effectively_zero(original) ? 1.0 : (target / original);
}

} // namespace

std::shared_ptr<Recipe> RecipeBooster::boost_recipe(const Recipe& original,
                                                    const IngredientStore& store,
                                                    units::quantity<units::percent> boost)
{
    // First pass: collect volume totals by category
    TotalsVisitor collector{store};
    for (const auto& step : original.production_steps()) {
        for (const auto& cmd : step) {
            cmd->accept(collector);
        }
    }

    const units::Litre orig_boostable = collector.boostable();
    const units::Litre orig_reducible = collector.reducible();
    const units::Litre variable_total = orig_boostable + orig_reducible;

    // Convert percentage to scaling factor
    const double boost_factor = boost.numerical_value_in(units::percent) * kPercentToFraction;

    // Calculate new volumes while preserving total variable volume
    auto new_boostable_total = orig_boostable * (1.0 + boost_factor);
    auto desired_reducible_total = variable_total - new_boostable_total;

    // Ensure reducible volume doesn't go negative
    if (desired_reducible_total < kZeroVolume) {
        desired_reducible_total = kZeroVolume;
        new_boostable_total = variable_total;
    }

    // Calculate scaling factors
    const auto scale_boostable = calculate_scale_factor(orig_boostable, new_boostable_total);
    const auto scale_reducible = calculate_scale_factor(orig_reducible, desired_reducible_total);

    // Second pass: apply scaling and build new recipe
    ProductionSteps boosted_steps;
    boosted_steps.reserve(original.production_steps().size());

    for (const auto& step : original.production_steps()) {
        Steps boosted_step;
        boosted_step.reserve(step.size());

        for (const auto& cmd : step) {
            ApplyVisitor applier(store, scale_boostable, scale_reducible);
            cmd->accept(applier);
            boosted_step.emplace_back(std::move(applier.take_command()));
        }
        boosted_steps.emplace_back(std::move(boosted_step));
    }

    return std::make_shared<Recipe>(original.name(),
                                    std::move(boosted_steps),
                                    original.description(),
                                    original.image_path(),
                                    original.nominal_serving_volume());
}

bool RecipeBooster::is_boostable(const Recipe& recipe, const IngredientStore& store)
{
    TotalsVisitor collector{store};

    for (const auto& step : recipe.production_steps()) {
        for (const auto& cmd : step) {
            cmd->accept(collector);
        }
    }

    const units::Litre boostable_total = collector.boostable();
    const units::Litre reducible_total = collector.reducible();

    // Recipe is boostable only if it contains both boostable and reducible ingredients
    // This ensures there's a meaningful ratio that can be adjusted
    return not is_effectively_zero(boostable_total) and not is_effectively_zero(reducible_total);
}
} // namespace cm
