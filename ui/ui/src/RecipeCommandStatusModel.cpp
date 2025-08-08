// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/ui/RecipeCommandStatusModel.hpp"

namespace cm::ui
{
RecipeCommandStatusModel::RecipeCommandStatusModel() = default;

QHash<int, QByteArray> RecipeCommandStatusModel::roleNames() const
{
    QHash<int, QByteArray> roles{
        {std::to_underlying(Roles::name), "name"},
        {std::to_underlying(Roles::status), "status"},
    };
    return roles;
}

int RecipeCommandStatusModel::rowCount([[maybe_unused]] const QModelIndex &parent) const
{
    return static_cast<int>(commands_.size());
}

QVariant RecipeCommandStatusModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= commands_.size())
    {
        return QVariant{};
    }
    auto &&cmd = *std::next(commands_.begin(), index.row());
    switch (Roles{role})
    {
    case Roles::name:
        return cmd.second.name;

    case Roles::status:
        return QVariant::fromValue(cmd.second.status);
    }
    return QVariant{};
}

void RecipeCommandStatusModel::reset()
{
    beginResetModel();
    commands_.clear();
    endResetModel();
}

void RecipeCommandStatusModel::register_command(CommandId id, QString name)
{
    beginResetModel();
    commands_.emplace(id, Data{.name = std::move(name), .status = CommandStatus::NotStarted});
    endResetModel();
}

void RecipeCommandStatusModel::command_started(CommandId id)
{
    const auto it = commands_.find(id);
    const auto row = static_cast<int>(std::distance(commands_.begin(), it));
    it->second.status = CommandStatus::Started;
    Q_EMIT dataChanged(index(row), index(row), {std::to_underlying(Roles::status)});
}

void RecipeCommandStatusModel::command_finished(CommandId id)
{
    const auto it = commands_.find(id);
    const auto row = static_cast<int>(std::distance(commands_.begin(), it));
    it->second.status = CommandStatus::Finished;
    Q_EMIT dataChanged(index(row), index(row), {std::to_underlying(Roles::status)});
}

} // namespace cm::ui
