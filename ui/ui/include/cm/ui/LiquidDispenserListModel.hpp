// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QAbstractItemModel>
#include <QtQmlIntegration>
#include "ExecutionContextAdapter.hpp"

namespace cm::ui {
class LiquidDispenserListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(cm::ui::ExecutionContextAdapter* executionContextAdapter MEMBER execution_context_adapter_ NOTIFY
                   execution_context_adapter_changed REQUIRED)
  public:
    enum class Roles
    {
        name_role = Qt::UserRole + 1,
    };
    LiquidDispenserListModel();
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

  Q_SIGNALS:
    void execution_context_adapter_changed();

  private:
    void init_model();

  private:
    ExecutionContextAdapter* execution_context_adapter_{nullptr};
    std::vector<QString> dispenser_names_;
};
} // namespace cm::ui
