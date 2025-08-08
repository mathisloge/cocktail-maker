#pragma once
#include <map>
#include <memory>
#include "glass.hpp"

namespace cm {
class GlassStore : public std::enable_shared_from_this<GlassStore>
{
  public:
    using GlassName = std::string;
    const Glass& add_glass(Glass glass, units::Grams glass_weight);
    const Glass& find_glass_by_weight(units::Grams glass_weight);

  private:
    using WeightGlassMap = std::map<units::Grams, Glass>;
    WeightGlassMap glasses_;
};
} // namespace cm
