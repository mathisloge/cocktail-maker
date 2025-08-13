// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <map>
#include <memory>
#include "glass.hpp"

namespace cm {
class GlassStore : public std::enable_shared_from_this<GlassStore>
{
  public:
    using WeightGlassMap = std::map<units::Grams, Glass>;

    const Glass& add_glass(Glass glass, units::Grams glass_weight);
    const Glass& find_glass_by_weight(units::Grams glass_weight);
    const WeightGlassMap& glasses() const;

  private:
    WeightGlassMap glasses_;
};
} // namespace cm
