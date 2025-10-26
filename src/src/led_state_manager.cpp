#include "cm/led_state_manager.hpp"
#include "cm/overloads.hpp"

namespace cm {
LedStateManager::LedStateManager(std::shared_ptr<ExecutionContext> execution_context, std::shared_ptr<WledSerial> leds)
    : leds_{std::move(leds)}
{
    execution_context->event_bus().subscribe([this](auto&& event) {
        std::visit(overloads{
                       [this](const ManualActionEvent& ev) {

                       },
                       [this](const RefillIngredientEvent& ev) {
                           update_state_for_ingredient(ev.ingredient_id, WledSerial::State::empty_ingredient);
                       },
                       [this]([[maybe_unused]] const ExecutionCanceledEvent& ev) { leds_->reset(); },
                       [this]([[maybe_unused]] RecipeFinishedEvent ev) {},
                       [this]([[maybe_unused]] const CommandStarted& ev) {
                           if (ev.ingredient_id.has_value()) {
                               update_state_for_ingredient(*ev.ingredient_id, WledSerial::State::active);
                           }
                       },
                       [this]([[maybe_unused]] const CommandProgress& ev) {},
                       [this]([[maybe_unused]] const CommandFinished& ev) {
                           if (ev.ingredient_id.has_value()) {
                               update_state_for_ingredient(*ev.ingredient_id, WledSerial::State::inactive);
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
