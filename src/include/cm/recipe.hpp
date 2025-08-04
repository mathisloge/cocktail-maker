#pragma once
#include <memory>
#include <vector>

namespace cm
{
class Command;
class Recipe
{
  public:
    using ProductionStep = std::vector<std::unique_ptr<Command>>;
    using ProductionSteps = std::vector<ProductionStep>;

    Recipe(std::string name);
    ~Recipe();

    const ProductionSteps &production_steps() const;

  private:
    ProductionSteps steps_;
};
} // namespace cm
