#pragma once
#include "cm/events/event_bus.hpp"
namespace cm
{
class CommandEventPublisher final
{
  public:
    CommandEventPublisher(EventBus &bus, CommandId command_id);
    ~CommandEventPublisher();
    void progress(units::quantity<units::percent> percentage);

  private:
    EventBus &bus_;
    CommandId command_id_;
};
} // namespace cm
