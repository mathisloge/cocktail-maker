// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QAbstractItemModel>
#include <QtQmlIntegration>
#include "cm/glass_store.hpp"

namespace cm::ui {
class GlassListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(cm::GlassStore* glassStore READ glass_store WRITE set_glass_store NOTIFY glass_store_changed)
  public:
    enum class Roles
    {
        name = Qt::UserRole + 1,
        capacity,
        image_source
    };
    GlassListModel();
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    GlassStore* glass_store();
    void set_glass_store(GlassStore* glass_store);

  Q_SIGNALS:
    void glass_store_changed();

  private:
    std::shared_ptr<GlassStore> glass_store_;
};
} // namespace cm::ui
