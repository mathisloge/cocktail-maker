#pragma once
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <cm/liquid_dispenser_stepper_pump.hpp>
#include "cm/execution_context.hpp"

namespace cm::ui {

// Works currently only for the stepper pump dispenser
class LiquidDispenserModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Can only be created by a factory like the ExecutionContextAdapter")
    Q_PROPERTY(bool running READ is_running NOTIFY running_changed)
    Q_PROPERTY(QString steps_per_litre READ calculate_steps_per_litre NOTIFY data_changed)
  public:
    LiquidDispenserModel(std::shared_ptr<ExecutionContext> execution_context, StepperPumpLiquidDispenser& pump);
    Q_INVOKABLE void run(int steps);
    Q_INVOKABLE void update_pumped(int milli_litre);
    Q_INVOKABLE void stop();

    bool is_running() const;
    QString calculate_steps_per_litre() const;

  Q_SIGNALS:
    void running_changed();
    void data_changed();

  public:
    std::shared_ptr<ExecutionContext> execution_context_;
    StepperPumpLiquidDispenser& pump_;
    boost::asio::cancellation_signal recipe_cancellation_;
    bool running_{};
    units::Steps steps_taken_{};
    units::quantity<units::milli_litre> pumped_{};
};
} // namespace cm::ui
