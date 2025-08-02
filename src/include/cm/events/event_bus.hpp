#pragma once
#include <variant>
#include <boost/signals2/signal.hpp>
#include "execution_canceled_event.hpp"
#include "manual_action_event.hpp"
#include "refill_ingredient_event.hpp"

namespace cm
{
using Event = std::variant<ManualActionEvent, RefillIngredientEvent, ExecutionCanceledEvent>;
using EventSignal = boost::signals2::signal<void(const Event &)>;

class EventBus
{
  public:
    boost::signals2::connection subscribe(const EventSignal::slot_type &slot);
    void publish(const Event &e);

  private:
    EventSignal signal_;
};
} // namespace cm
