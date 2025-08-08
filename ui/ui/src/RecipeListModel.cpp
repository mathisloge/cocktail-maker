// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/ui/RecipeListModel.hpp"
#include <cm/recipe.hpp>

namespace cm::ui {
RecipeListModel::RecipeListModel()
{
    connect(this, &RecipeListModel::recipeStoreChanged, this, [this]() { Q_EMIT dataChanged(index(0), index(rowCount({}))); });
}

QHash<int, QByteArray> RecipeListModel::roleNames() const
{
    QHash<int, QByteArray> roles{
        {std::to_underlying(Roles::name_role), "name"},
        {std::to_underlying(Roles::image_source_role), "imageSource"},
    };
    return roles;
}

int RecipeListModel::rowCount([[maybe_unused]] const QModelIndex& parent) const
{
    if (recipe_store_ == nullptr) {
        return 0;
    }
    return static_cast<int>(recipe_store_->recipes().size());
}

QVariant RecipeListModel::data(const QModelIndex& index, int role) const
{
    if (recipe_store_ == nullptr) {
        return QVariant{};
    }
    auto&& recipes = recipe_store_->recipes();
    if (index.row() >= recipes.size()) {
        return QVariant{};
    }
    auto&& recipe = *std::next(recipes.begin(), index.row());
    switch (Roles{role}) {
    case Roles::name_role:
        return QString::fromStdString(recipe.second->name());

    case Roles::image_source_role:
        return "qrc:/qt/qml/CocktailMaker/Ui/mojito.png";
    }
    return QVariant{};
}

} // namespace cm::ui
