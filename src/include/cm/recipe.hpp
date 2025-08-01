#pragma once
#include <memory>
#include <string>
#include "liquid_dispenser.hpp"
#include <mp-units/systems/si.h>

namespace cm
{

struct Ingredient
{
    std::string name;
};

struct LiquidIngredient : public Ingredient
{};

struct ManualIngredient : public Ingredient
{
    std::string instructions;
};

class IngredientRepository
{
  public:
    void add_ingredient(LiquidIngredient liquid, std::unique_ptr<LiquidDispenser> dispenser);
    void add_ingredient(ManualIngredient ingredient);

    void refill_ingredient(std::string name, mp_units::quantity<mp_units::si::litre> new_volume);
    void dispense_ingredient(std::string name, mp_units::quantity<mp_units::si::litre> volume);
};

struct RecipeLiquidComponent
{
    std::string ingredient;
    mp_units::quantity<mp_units::si::litre> volume;
};

struct ManualComponent
{
    std::string ingredient;
};
struct ProductionStep
{
    using Component = std::variant<RecipeLiquidComponent, ManualComponent>;
    std::vector<Component> ingredient;
};

class Recipe
{
    std::vector<ProductionStep> steps_;
};
} // namespace cm
