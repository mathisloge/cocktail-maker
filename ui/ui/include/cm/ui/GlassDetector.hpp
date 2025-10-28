// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <cm/glass_store.hpp>
#include "GlassAdapter.hpp"

namespace cm::ui {
class GlassDetector : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(cm::GlassStore* glassStore READ glass_store WRITE set_glass_store NOTIFY glass_store_changed);
    Q_PROPERTY(cm::ui::GlassAdapter detectedGlass READ detected_glass NOTIFY detected_glass_changed);

  public:
    cm::ui::GlassAdapter detected_glass() const;

    GlassStore* glass_store();
    void set_glass_store(GlassStore* glass_store);

  Q_SIGNALS:
    void glass_store_changed();
    void detected_glass_changed();

  private:
    std::shared_ptr<GlassStore> glass_store_;
};
} // namespace cm::ui
