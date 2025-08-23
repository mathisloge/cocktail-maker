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
    using GlassMap = std::map<GlassId, Glass>;

    const Glass& add_glass(Glass glass, units::Grams glass_weight);
    const Glass& find_glass_by_id(const GlassId& id) const;
    const Glass& find_glass_by_weight(units::Grams glass_weight) const;
    const GlassMap& glasses() const;

  private:
    GlassMap glasses_;
    std::map<units::Grams, GlassId> glass_weights_;
};
} // namespace cm
