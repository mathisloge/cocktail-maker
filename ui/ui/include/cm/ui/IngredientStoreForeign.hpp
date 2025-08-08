#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/ingredient_store.hpp>

namespace cm::ui
{
struct IngredientStoreForeign
{
    Q_GADGET
    QML_FOREIGN(cm::IngredientStore *)
    QML_NAMED_ELEMENT(IngredientStore)
};
} // namespace cm::ui
