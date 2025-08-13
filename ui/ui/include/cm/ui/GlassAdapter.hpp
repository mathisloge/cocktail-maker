#pragma once
#include <QObject>
#include <QtQmlIntegration>
#include <cm/glass.hpp>

namespace cm::ui {
struct GlassAdapter
{
    Q_GADGET
    QML_VALUE_TYPE(glass)
    Q_PROPERTY(QString displayName READ display_name CONSTANT)
    Q_PROPERTY(QString imagePath READ image_path CONSTANT)
    Q_PROPERTY(QString capacity READ capacity CONSTANT)

  public:
    GlassAdapter(cm::Glass glass = {})
        : glass{std::move(glass)}
    {
    }

    QString display_name() const
    {
        return QString::fromStdString(glass.display_name);
    }

    QString image_path() const
    {
        return QString::fromStdString(glass.image_path.string());
    }

    QString capacity() const
    {
        return QString::fromStdString(fmt::format("{}", glass.capacity.in(units::milli_litre)));
    }

    cm::Glass glass;
};
} // namespace cm::ui
