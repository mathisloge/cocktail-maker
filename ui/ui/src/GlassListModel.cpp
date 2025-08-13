// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GlassListModel.hpp"

namespace cm::ui {
GlassListModel::GlassListModel()
{
    connect(this, &GlassListModel::glass_store_changed, this, [this]() { Q_EMIT dataChanged(index(0), index(rowCount({}))); });
}

QHash<int, QByteArray> GlassListModel::roleNames() const
{
    QHash<int, QByteArray> roles{
        {std::to_underlying(Roles::name), "name"},
        {std::to_underlying(Roles::capacity), "capacity"},
        {std::to_underlying(Roles::image_source), "imageSource"},
    };
    return roles;
}

int GlassListModel::rowCount([[maybe_unused]] const QModelIndex& parent) const
{
    if (glass_store_ == nullptr) {
        return 0;
    }
    return static_cast<int>(glass_store_->glasses().size());
}

QVariant GlassListModel::data(const QModelIndex& index, int role) const
{
    if (glass_store_ == nullptr) {
        return QVariant{};
    }
    auto&& glasses = glass_store_->glasses();
    if (index.row() >= glasses.size()) {
        return QVariant{};
    }
    auto&& recipe = *std::next(glasses.begin(), index.row());
    switch (Roles{role}) {
    case Roles::name:
        return QString::fromStdString(recipe.second.display_name);
    case Roles::capacity:
        return QString::fromStdString(fmt::format("{}", recipe.second.capacity.in(units::si::milli<units::si::litre>)));

    case Roles::image_source:
        return "qrc:/qt/qml/CocktailMaker/Ui/mojito.png";
    }
    return QVariant{};
}

GlassStore* GlassListModel::glass_store()
{
    return glass_store_.get();
}

void GlassListModel::set_glass_store(GlassStore* glass_store)
{
    if (glass_store_.get() != glass_store) {
        glass_store_ = (glass_store != nullptr) ? glass_store->shared_from_this() : nullptr;
        Q_EMIT glass_store_changed();
    }
}

} // namespace cm::ui
