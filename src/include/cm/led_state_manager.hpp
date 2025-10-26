#pragma once
#include "cm/execution_context.hpp"
#include "cm/hw/wled_serial.hpp"

namespace cm {
class LedStateManager
{
  public:
    LedStateManager(std::shared_ptr<ExecutionContext> execution_context, std::shared_ptr<WledSerial> leds);
    void connect_led_segment_with_ingredient(std::uint8_t segment_id, IngredientId ingredient_id);

  private:
    void update_state_for_ingredient(IngredientId ingredient_id, WledSerial::State new_state);

  private:
    std::shared_ptr<ExecutionContext> execution_context_;
    std::shared_ptr<WledSerial> leds_;
    std::unordered_map<IngredientId, std::uint8_t> ingredient_led_segment_map_;
};
} // namespace cm
