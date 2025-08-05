#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/recipe_store.hpp>

namespace cm::ui
{
struct RecipeStoreForeign
{
    Q_GADGET
    QML_FOREIGN(cm::RecipeStore *)
    QML_NAMED_ELEMENT(RecipeStore)
    QML_UNCREATABLE("Provided by c++ API")
};
} // namespace cm::ui
