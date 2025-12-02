#include "cm/ui/LiquidDispenserModel.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <cm/logging.hpp>

namespace cm::ui {
LiquidDispenserModel::LiquidDispenserModel(std::shared_ptr<ExecutionContext> execution_context, StepperPumpLiquidDispenser& pump)
    : execution_context_{std::move(execution_context)}
    , pump_{pump}
{
}

void LiquidDispenserModel::run(int steps)
{
    steps_taken_ = steps;
    Q_EMIT running_changed();

    boost::asio::co_spawn(
        execution_context_->async_executor(),
        [this, ctx = execution_context_] -> boost::asio::awaitable<void> {
            running_ = true;
            Q_EMIT running_changed();

            co_await pump_.dispense(steps_taken_);

            running_ = false;
            Q_EMIT running_changed();
        },
        boost::asio::bind_cancellation_slot(recipe_cancellation_.slot(), boost::asio::detached));

    Q_EMIT data_changed();
}

bool LiquidDispenserModel::is_running() const
{
    return running_;
}

void LiquidDispenserModel::update_pumped(int milli_litre)
{
    const units::quantity<units::milli_litre> val = milli_litre * units::milli_litre;

    if (pumped_ != val) {
        pumped_ = val;
        Q_EMIT data_changed();
    }
}

void LiquidDispenserModel::stop()
{
#undef emit
    recipe_cancellation_.emit(boost::asio::cancellation_type::total);
    running_ = false;
    Q_EMIT running_changed();
}

QString LiquidDispenserModel::calculate_steps_per_litre() const
{
    if (units::abs(pumped_) < std::numeric_limits<units::Litre>::epsilon()) {

        return QString::fromStdString(fmt::format("{}", (units::step / units::si::litre)));
    }
    units::StepsPerLitre steps_per_litre = (steps_taken_ / pumped_) * 1000;

    return QString::fromStdString(fmt::format("{}", steps_per_litre));
}
} // namespace cm::ui
