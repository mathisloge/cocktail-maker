#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/ingredient_store.hpp>
#include <cm/recipe_store.hpp>
#include "RecipeDetail.hpp"

namespace cm::ui
{
class RecipeBoosterAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(cm::IngredientStore *ingredientStore WRITE set_ingredient_store READ get_ingredient_store NOTIFY
                   ingredient_store_changed)
    Q_PROPERTY(cm::ui::RecipeDetail *originalRecipe MEMBER original_recipe_ NOTIFY original_recipe_changed)
    Q_PROPERTY(bool isBoostable READ is_boostable NOTIFY original_recipe_changed)
  public:
    // Raw-pointer as qml takes ownership
    Q_INVOKABLE cm::ui::RecipeDetail *boost(std::int32_t percentage);
    bool is_boostable() const;

    void set_ingredient_store(cm::IngredientStore *ingredient_store);
    cm::IngredientStore *get_ingredient_store();

  Q_SIGNALS:
    void ingredient_store_changed();
    void original_recipe_changed();

  private:
    std::shared_ptr<IngredientStore> ingredient_store_;
    cm::ui::RecipeDetail *original_recipe_;
};
} // namespace cm::ui
