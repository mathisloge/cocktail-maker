// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/execution_context.hpp>
#include <cm/recipe_executor.hpp>
#include "RecipeCommandStatusModel.hpp"
#include "RecipeDetail.hpp"
#include "cm/ingredient_store.hpp"

namespace cm::ui {
class RecipeExecutorAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by ApplicationState")
    Q_PROPERTY(cm::ui::RecipeCommandStatusModel* commandStatusModel READ command_status_model CONSTANT)
  public:
    explicit RecipeExecutorAdapter(std::shared_ptr<ExecutionContext> ctx,
                                   std::shared_ptr<const IngredientStore> ingredient_store);
    Q_INVOKABLE void make_recipe(RecipeDetail* recipe, QString glassId);
    Q_INVOKABLE void continue_mix();
    Q_INVOKABLE void cancel();

    RecipeCommandStatusModel* command_status_model();

  Q_SIGNALS:
    void manualActionRequired(QString instruction);
    void refillActionRequired(QString ingredient); //! TODO: change this to ingredient value type
    void executionAborted();
    void finished();

  private:
    std::shared_ptr<ExecutionContext> ctx_;
    std::shared_ptr<const IngredientStore> ingredient_store_;
    std::unique_ptr<RecipeExecutor> executor_;
    RecipeCommandStatusModel command_model_;
};
} // namespace cm::ui
