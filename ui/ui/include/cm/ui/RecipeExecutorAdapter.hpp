#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/execution_context.hpp>
#include <cm/recipe_executor.hpp>
#include "RecipeDetail.hpp"
#include "cm/ingredient_store.hpp"

namespace cm::ui
{
class RecipeExecutorAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by ApplicationState")
  public:
    explicit RecipeExecutorAdapter(std::shared_ptr<ExecutionContext> ctx,
                                   std::shared_ptr<const IngredientStore> ingredient_store);
    Q_INVOKABLE void make_recipe(RecipeDetail *recipe);
    Q_INVOKABLE void continue_mix();
    Q_INVOKABLE void cancel();

  Q_SIGNALS:
    void manualActionRequired(QString instruction);
    void refillActionRequired(QString ingredient); //! TODO: change this to ingredient value type
    void executionAborted();
    void finished();

  private:
    std::shared_ptr<ExecutionContext> ctx_;
    std::shared_ptr<const IngredientStore> ingredient_store_;
    std::unique_ptr<RecipeExecutor> executor_;
};
} // namespace cm::ui
