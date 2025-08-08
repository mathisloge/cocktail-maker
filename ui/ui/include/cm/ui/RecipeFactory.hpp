// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include "RecipeDetail.hpp"
#include "cm/ingredient_store.hpp"
#include "cm/recipe_store.hpp"
namespace cm::ui
{
class RecipeFactory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by ApplicationState")
  public:
    explicit RecipeFactory(std::shared_ptr<RecipeStore> recipe_store,
                           std::shared_ptr<const IngredientStore> ingredient_store);
    // Raw-pointer as qml takes ownership
    Q_INVOKABLE RecipeDetail *create(const QString &recipeName);

  private:
    std::shared_ptr<RecipeStore> recipe_store_;
    std::shared_ptr<const IngredientStore> ingredient_store_;
};
} // namespace cm::ui
