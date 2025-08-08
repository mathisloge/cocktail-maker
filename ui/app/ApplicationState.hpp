
#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QtQmlIntegration>
#include <cm/ingredient_store.hpp>
#include <cm/recipe_store.hpp>
#include <cm/ui/RecipeExecutorAdapter.hpp>
#include <cm/ui/RecipeFactory.hpp>
namespace cm::app
{
class ApplicationState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(cm::RecipeStore *recipeStore READ get_recipe_store CONSTANT);
    Q_PROPERTY(cm::IngredientStore *ingredientStore READ get_ingredient_store CONSTANT);
    Q_PROPERTY(cm::ui::RecipeFactory *recipeFactory READ get_recipe_factory CONSTANT)
    Q_PROPERTY(cm::ui::RecipeExecutorAdapter *recipeExecutor READ get_recipe_executor CONSTANT)

  public:
    RecipeStore *get_recipe_store()
    {
        return recipe_store.get();
    }

    IngredientStore *get_ingredient_store()
    {
        return ingredient_store.get();
    }

    ui::RecipeFactory *get_recipe_factory()
    {
        return recipe_factory.get();
    }

    ui::RecipeExecutorAdapter *get_recipe_executor() const
    {
        return recipe_executor.get();
    }

  public:
    std::shared_ptr<RecipeStore> recipe_store;
    std::shared_ptr<IngredientStore> ingredient_store;
    std::shared_ptr<ui::RecipeFactory> recipe_factory;
    std::shared_ptr<ui::RecipeExecutorAdapter> recipe_executor;
};
} // namespace cm::app
