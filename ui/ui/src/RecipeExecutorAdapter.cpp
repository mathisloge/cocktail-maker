// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/ui/RecipeExecutorAdapter.hpp"
#include <cm/overloads.hpp>
#include "cm/commands/command.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"

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
        name = QString::fromStdString(ingredient.display_name);
    }

    void visit([[maybe_unused]] const ManualCmd& cmd) override
    {
        name = "Manuell"; // codespell:ignore
    }

    const IngredientStore& ingredient_store;
    QString name;
};
} // namespace

RecipeExecutorAdapter::RecipeExecutorAdapter(std::shared_ptr<ExecutionContext> ctx,
                                             std::shared_ptr<const IngredientStore> ingredient_store)
    : ctx_{std::move(ctx)}
    , ingredient_store_{std::move(ingredient_store)}
{
    ctx_->event_bus().subscribe([self = QPointer{this}](auto&& event) {
        if (self.isNull()) {
            return;
        }

        std::visit(overloads{
                       [self](const ManualActionEvent& ev) {
                           Q_EMIT self->manualActionRequired(QString::fromStdString(ev.instruction));
                       },
                       [self](const RefillIngredientEvent& ev) {
                           auto&& ingredient = self->ingredient_store_->find_ingredient(ev.ingredient_id);
                           Q_EMIT self->refillActionRequired(QString::fromStdString(ingredient.display_name));
                       },
                       [self]([[maybe_unused]] const ExecutionCanceledEvent& ev) { Q_EMIT self->executionAborted(); },
                       [self]([[maybe_unused]] RecipeFinishedEvent ev) { Q_EMIT self->finished(); },
                       [self]([[maybe_unused]] const CommandStarted& ev) { self->command_model_.command_started(ev.cmd_id); },
                       [self]([[maybe_unused]] const CommandProgress& ev) {},
                       [self]([[maybe_unused]] const CommandFinished& ev) { self->command_model_.command_finished(ev.cmd_id); },
                   },
                   event);
    });
}

void RecipeExecutorAdapter::make_recipe(RecipeDetail* recipe)
{
    command_model_.reset();
    executor_ = std::make_unique<RecipeExecutor>(ctx_, recipe->recipe());
    for (auto&& steps : recipe->recipe()->production_steps()) {
        for (auto&& cmd : steps) {
            CommandUiFormatter name_formatter{*ingredient_store_};
            cmd->accept(name_formatter);
            command_model_.register_command(cmd->id(), name_formatter.name);
        }
    }
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

RecipeCommandStatusModel* RecipeExecutorAdapter::command_status_model()
{
    return &command_model_;
}
} // namespace cm::ui
