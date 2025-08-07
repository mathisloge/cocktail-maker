#include "cm/ui/RecipeExecutorAdapter.hpp"
#include <cm/overloads.hpp>

namespace cm::ui
{
RecipeExecutorAdapter::RecipeExecutorAdapter(std::shared_ptr<ExecutionContext> ctx,
                                             std::shared_ptr<const IngredientStore> ingredient_store)
    : ctx_{std::move(ctx)}
    , ingredient_store_{std::move(ingredient_store)}
{
    ctx_->event_bus().subscribe([self = QPointer{this}](auto &&event) {
        if (self.isNull())
        {
            return;
        }

        std::visit(overloads{
                       [self](const ManualActionEvent &ev) {
                           Q_EMIT self->manualActionRequired(QString::fromStdString(ev.instruction));
                       },
                       [self](const RefillIngredientEvent &ev) {
                           auto &&ingredient = self->ingredient_store_->find_ingredient(ev.ingredient_id);
                           Q_EMIT self->refillActionRequired(QString::fromStdString(ingredient.display_name));
                       },
                       [self]([[maybe_unused]] const ExecutionCanceledEvent &ev) { Q_EMIT self->executionAborted(); },
                       [self]([[maybe_unused]] RecipeFinishedEvent ev) { Q_EMIT self->finished(); },
                       [self]([[maybe_unused]] const CommandStarted &ev) {},
                       [self]([[maybe_unused]] const CommandProgress &ev) {},
                       [self]([[maybe_unused]] const CommandFinished &ev) {},
                   },
                   event);
    });
}

void RecipeExecutorAdapter::make_recipe(RecipeDetail *recipe)
{
    executor_ = std::make_unique<RecipeExecutor>(ctx_, recipe->recipe());
    executor_->run();
}

void RecipeExecutorAdapter::continue_mix()
{
    executor_->continue_execution();
}

void RecipeExecutorAdapter::cancel()
{
    executor_ = nullptr;
}
} // namespace cm::ui
