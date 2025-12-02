// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/ui/LiquidDispenserListModel.hpp"
#include <cm/recipe.hpp>

namespace cm::ui {
LiquidDispenserListModel::LiquidDispenserListModel()
{
    connect(this, &LiquidDispenserListModel::execution_context_adapter_changed, this, &LiquidDispenserListModel::init_model);
}

QHash<int, QByteArray> LiquidDispenserListModel::roleNames() const
{
    QHash<int, QByteArray> roles{
        {std::to_underlying(Roles::name_role), "name"},
    };
    return roles;
}

int LiquidDispenserListModel::rowCount([[maybe_unused]] const QModelIndex& parent) const
{
    if (execution_context_adapter_ == nullptr) {
        return 0;
    }
    return static_cast<int>(dispenser_names_.size());
}

QVariant LiquidDispenserListModel::data(const QModelIndex& index, int role) const
{
    if (execution_context_adapter_ == nullptr) {
        return QVariant{};
    }

    if (index.row() >= dispenser_names_.size()) {
        return QVariant{};
    }
    switch (Roles{role}) {
    case Roles::name_role:
        return dispenser_names_.at(index.row());
    }
    return QVariant{};
}

void LiquidDispenserListModel::init_model()
{
    beginResetModel();
    dispenser_names_.clear();
    if (execution_context_adapter_ != nullptr) {
        auto&& registry = execution_context_adapter_->context().liquid_registry();
        auto vec = registry.get_dispensers() | std::views::transform([](LiquidDispenser& d) { return &d; }) |
                   std::ranges::to<std::vector>();
        std::ranges::sort(vec, {}, [](auto& d) { return d->name(); });
        dispenser_names_.reserve(vec.size());
        std::ranges::for_each(vec, [this](LiquidDispenser* dispenser) {
            dispenser_names_.emplace_back(QString::fromStdString(dispenser->name()));
        });
    }

    endResetModel();
}

} // namespace cm::ui
