// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QAbstractItemModel>
#include <QtQmlIntegration>
#include "cm/recipe_store.hpp"

namespace cm::ui {
class RecipeListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(cm::RecipeStore* recipeStore MEMBER recipe_store_ NOTIFY recipeStoreChanged)
  public:
    enum class Roles
    {
        name_role = Qt::UserRole + 1,
        image_source_role
    };
    RecipeListModel();
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

  Q_SIGNALS:
    void recipeStoreChanged();

  private:
    RecipeStore* recipe_store_{nullptr};
};
} // namespace cm::ui
