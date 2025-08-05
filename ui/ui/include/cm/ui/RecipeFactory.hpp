#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include "RecipeDetail.hpp"
#include "cm/recipe_store.hpp"
namespace cm::ui
{
class RecipeFactory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by ApplicationState")
  public:
    explicit RecipeFactory(std::shared_ptr<RecipeStore> recipe_store);
    // Raw-pointer as qml takes ownership
    Q_INVOKABLE RecipeDetail *create(const QString &recipeName);

  private:
    std::shared_ptr<RecipeStore> recipe_store_;
};
} // namespace cm::ui
