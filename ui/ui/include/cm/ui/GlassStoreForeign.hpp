// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/glass_store.hpp>

namespace cm::ui {
struct GlassStoreForeign
{
    Q_GADGET
    QML_FOREIGN(cm::GlassStore*)
    QML_NAMED_ELEMENT(GlassStore)
};
} // namespace cm::ui
