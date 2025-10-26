#include "cm/led_state_manager.hpp"
#include "cm/overloads.hpp"

namespace cm {
LedStateManager::LedStateManager(std::shared_ptr<WledSerial> leds)
    : leds_{std::move(leds)}
{
}

void LedStateManager::subscribe_to_events(EventBus& event_bus)
{
    event_bus.subscribe([self = shared_from_this()](auto&& event) {
        std::visit(overloads{
                       [](const ManualActionEvent& ev) {

                       },
                       [self](const RefillIngredientEvent& ev) {
                           self->update_state_for_ingredient(ev.ingredient_id, WledSerial::State::empty_ingredient);
                       },
                       [self]([[maybe_unused]] const ExecutionCanceledEvent& ev) { self->leds_->reset(); },
                       []([[maybe_unused]] RecipeFinishedEvent ev) {},
                       [self]([[maybe_unused]] const CommandStarted& ev) {
                           if (ev.ingredient_id.has_value()) {
                               self->update_state_for_ingredient(*ev.ingredient_id, WledSerial::State::active);
                           }
                       },
                       []([[maybe_unused]] const CommandProgress& ev) {},
                       [self]([[maybe_unused]] const CommandFinished& ev) {
                           if (ev.ingredient_id.has_value()) {
                               self->update_state_for_ingredient(*ev.ingredient_id, WledSerial::State::inactive);
                           }
                       },
                   },
                   event);
    });
}

void LedStateManager::connect_led_segment_with_ingredient(std::uint8_t segment_id, IngredientId ingredient_id)
{
    ingredient_led_segment_map_[ingredient_id] = segment_id;
}

void LedStateManager::update_state_for_ingredient(IngredientId ingredient_id, WledSerial::State new_state)
{
    auto it = ingredient_led_segment_map_.find(ingredient_id);
    if (it != ingredient_led_segment_map_.end()) {
        leds_->set_state(it->second, new_state);
    }
}
} // namespace cm
