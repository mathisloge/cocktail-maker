#include "cm/ui/ExecutionContextAdapter.hpp"
#include <libassert/assert.hpp>

namespace cm::ui {

ExecutionContextAdapter::ExecutionContextAdapter(std::shared_ptr<ExecutionContext> ctx)
    : ctx_{std::move(ctx)}
{
}

ExecutionContext& ExecutionContextAdapter::context()
{
    ASSERT(ctx_ != nullptr);
    return *ctx_;
}

LiquidDispenserModel* ExecutionContextAdapter::create_dispenser_model(const QString& name)
{
    auto dispensers = ctx_->liquid_registry().get_dispensers();
    auto&& dispenser_it = std::ranges::find_if(dispensers, [str = name.toStdString()](auto&& d) { return d->name() == str; });
    if (dispenser_it != std::ranges::end(dispensers)) {
        auto* dispenser_stepper = dynamic_cast<StepperPumpLiquidDispenser*>(*dispenser_it);
        if (dispenser_stepper != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) will be owned by qml engine
            return new LiquidDispenserModel(ctx_, *dispenser_stepper);
        }
    }
    return nullptr;
}
} // namespace cm::ui
