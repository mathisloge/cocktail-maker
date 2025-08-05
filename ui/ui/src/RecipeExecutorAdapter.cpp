#include "cm/ui/RecipeExecutorAdapter.hpp"
#include <cm/overloads.hpp>

namespace cm::ui
{
RecipeExecutorAdapter::RecipeExecutorAdapter(std::shared_ptr<ExecutionContext> ctx)
    : ctx_{std::move(ctx)}
{
    ctx_->event_bus().subscribe([self = QPointer{this}](auto &&event) {
        if (self.isNull())
        {
            return;
        }

        std::visit(
            overloads{[self](const ManualActionEvent &ev) {
                          Q_EMIT self->manualActionRequired(QString::fromStdString(ev.instruction));
                      },
                      [self](const RefillIngredientEvent &ev) {
                          Q_EMIT self->refillActionRequired(QString::fromStdString(ev.ingredient_id));
                      },
                      [self]([[maybe_unused]] const ExecutionCanceledEvent &ev) { Q_EMIT self->executionAborted(); },
                      [self]([[maybe_unused]] RecipeFinishedEvent ev) {
                          QMetaObject::invokeMethod(self, &RecipeExecutorAdapter::finished, Qt::QueuedConnection);
                      }},
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
