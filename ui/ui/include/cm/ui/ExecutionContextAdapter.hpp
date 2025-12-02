#pragma once
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <cm/execution_context.hpp>

namespace cm::ui {
class ExecutionContextAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by ApplicationState")

  public:
    explicit ExecutionContextAdapter(std::shared_ptr<ExecutionContext> ctx);
    ExecutionContext& context();

  private:
    std::shared_ptr<ExecutionContext> ctx_;
};
} // namespace cm::ui
