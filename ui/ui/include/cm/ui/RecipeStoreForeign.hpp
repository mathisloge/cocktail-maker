// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
};
} // namespace cm::ui
