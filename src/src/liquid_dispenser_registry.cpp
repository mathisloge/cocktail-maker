#include "cm/liquid_dispenser_registry.hpp"

namespace cm
{
void LiquidDispenserRegistry::register_dispenser(IngredientId ingredient, std::unique_ptr<LiquidDispenser> dispenser)
{
    dispensers_.insert_or_assign(std::move(ingredient), std::move(dispenser));
}

LiquidDispenser &LiquidDispenserRegistry::dispenser(const IngredientId &ingredient)
{
    return *dispensers_.at(ingredient);
}

} // namespace cm
