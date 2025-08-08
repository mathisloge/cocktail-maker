// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
