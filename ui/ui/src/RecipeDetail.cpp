// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/ui/RecipeDetail.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"

namespace cm::ui {
namespace {
struct CommandUiFormatter final : public CommandVisitor
{
    CommandUiFormatter(const IngredientStore& ingredient_store)
        : ingredient_store{ingredient_store}
    {
    }

    void visit(const DispenseLiquidCmd& cmd) override
    {
        auto&& ingredient = ingredient_store.find_ingredient(cmd.ingredient());
        step.name = QString::fromStdString(ingredient.display_name);
        step.detail =
            QString::fromStdString(fmt::format("{}",
                                               units::value_cast<std::int32_t>(cmd.volume().in(
                                                   mp_units::si::milli<mp_units::si::litre>))));
    }

    void visit(const ManualCmd& cmd) override
    {
        step.name = "Manuell"; // codespell:ignore
        step.detail = QString::fromStdString(cmd.instruction());
    }

    const IngredientStore& ingredient_store;
    RecipeStepDetail step;
};
} // namespace

RecipeDetail::RecipeDetail(std::shared_ptr<Recipe> recipe,
                           std::shared_ptr<const IngredientStore> ingredient_store)
    : recipe_{std::move(recipe)}
    , ingredient_store_{std::move(ingredient_store)}
{
    for (auto&& steps : recipe_->production_steps()) {
        for (auto&& cmd : steps) {
            CommandUiFormatter formatter{*ingredient_store_};
            cmd->accept(formatter);
            steps_.emplace_back(std::move(formatter.step));
        }
    }
}

QString RecipeDetail::name() const
{
    return QString::fromStdString(recipe_->name());
}

QString RecipeDetail::description() const
{
    return QString::fromStdString(recipe_->description());
}

QString RecipeDetail::image_path() const
{
    return QString::fromStdString(recipe_->image_path());
}

const QList<cm::ui::RecipeStepDetail>& RecipeDetail::steps() const
{
    return steps_;
}

std::shared_ptr<Recipe> RecipeDetail::recipe() const
{
    return recipe_;
}

} // namespace cm::ui
