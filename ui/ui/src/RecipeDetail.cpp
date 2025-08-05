#include "cm/ui/RecipeDetail.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"

namespace cm::ui
{
namespace
{
struct StepUiFormatter final : public CommandVisitor
{
    void visit(const DispenseLiquidCmd &cmd)
    {
        step.name = QString::fromStdString(cmd.ingredient());
        step.detail =
            QString::fromStdString(fmt::format("{}", cmd.volume().in(mp_units::si::centi<mp_units::si::litre>)));
    }

    void visit(const ManualCmd &cmd)
    {
        step.name = "Manuell"; // codespell:ignore
        step.detail = QString::fromStdString(cmd.instruction());
    }
    RecipeStepDetail step;
};
} // namespace

RecipeDetail::RecipeDetail(std::shared_ptr<Recipe> recipe)
    : recipe_{std::move(recipe)}
{
    for (auto &&steps : recipe_->production_steps())
    {
        for (auto &&cmd : steps)
        {
            StepUiFormatter formatter;
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

const QList<cm::ui::RecipeStepDetail> &RecipeDetail::steps() const
{
    return steps_;
}

std::shared_ptr<Recipe> RecipeDetail::recipe() const
{
    return recipe_;
}

} // namespace cm::ui
