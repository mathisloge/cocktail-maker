// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "cm/events/event_bus.hpp"
#include "cm/hw/wled_serial.hpp"

namespace cm {
class LedStateManager : public std::enable_shared_from_this<LedStateManager>
{
  public:
    explicit LedStateManager(std::shared_ptr<WledSerial> leds);
    void subscribe_to_events(EventBus& event_bus);
    void connect_led_segment_with_ingredient(std::uint8_t segment_id, IngredientId ingredient_id);

  private:
    void update_state_for_ingredient(IngredientId ingredient_id, WledSerial::State new_state);

  private:
    std::shared_ptr<WledSerial> leds_;
    std::unordered_map<IngredientId, std::uint8_t> ingredient_led_segment_map_;
};
} // namespace cm
