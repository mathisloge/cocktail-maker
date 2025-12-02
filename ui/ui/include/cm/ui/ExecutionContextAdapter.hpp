#pragma once
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <cm/execution_context.hpp>
#include "LiquidDispenserModel.hpp"

namespace cm::ui {
class ExecutionContextAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by ApplicationState")

  public:
    explicit ExecutionContextAdapter(std::shared_ptr<ExecutionContext> ctx);
    ExecutionContext& context();

    Q_INVOKABLE LiquidDispenserModel* create_dispenser_model(const QString& name);

  private:
    std::shared_ptr<ExecutionContext> ctx_;
};
} // namespace cm::ui
