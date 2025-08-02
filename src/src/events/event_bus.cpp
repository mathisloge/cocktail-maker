#include "cm/events/event_bus.hpp"

namespace cm
{
boost::signals2::connection EventBus::subscribe(const EventSignal::slot_type &slot)
{
    return signal_.connect(slot);
}

void EventBus::publish(const Event &e)
{
    signal_(e);
}

} // namespace cm
