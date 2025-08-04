#pragma once
#include <boost/asio.hpp>
#include "events/event_bus.hpp"
#include "liquid_dispenser_registry.hpp"

namespace cm
{
class ExecutionContext
{
  public:
    explicit ExecutionContext(boost::asio::io_context &io);
    boost::asio::awaitable<void> wait_for_resume();
    void resume();
    EventBus &event_bus();
    LiquidDispenserRegistry &liquid_registry();
    boost::asio::io_context &io_context();

  private:
    boost::asio::io_context &io_context_;
    boost::asio::steady_timer resume_timer_;
    EventBus event_bus_;
    LiquidDispenserRegistry liquid_registry_;
};
} // namespace cm
