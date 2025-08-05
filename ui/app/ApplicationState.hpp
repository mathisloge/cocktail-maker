
#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/recipe_store.hpp>

namespace cm::app
{
class ApplicationState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(cm::RecipeStore *recipeStore READ get_recipe_store CONSTANT);

  public:
    RecipeStore *get_recipe_store()
    {
        return recipe_store.get();
    }

  public:
    std::shared_ptr<RecipeStore> recipe_store;
};
} // namespace cm::app
