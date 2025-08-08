// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio.hpp>
#include "events/event_bus.hpp"
#include "liquid_dispenser_registry.hpp"

namespace cm {
class ExecutionContext
{
  public:
    explicit ExecutionContext(boost::asio::any_io_executor executor);
    boost::asio::awaitable<void> wait_for_resume();
    void resume();
    EventBus& event_bus();
    LiquidDispenserRegistry& liquid_registry();
    boost::asio::any_io_executor async_executor();

  private:
    boost::asio::any_io_executor io_context_;
    boost::asio::steady_timer resume_timer_;
    EventBus event_bus_;
    LiquidDispenserRegistry liquid_registry_;
};
} // namespace cm
