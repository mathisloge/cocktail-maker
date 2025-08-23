// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/glass_store.hpp"

namespace cm {
const Glass& GlassStore::add_glass(Glass glass, units::Grams glass_weight)
{
    auto&& [it, _] = glasses_.emplace(glass.id, glass);
    glass_weights_.emplace(glass_weight, glass.id);
    return it->second;
}

const Glass& GlassStore::find_glass_by_id(const GlassId& id) const
{
    auto it = glasses_.find(id);
    if (it == glasses_.end()) {
        throw std::out_of_range("Could not find glass");
    }
    return it->second;
}

const Glass& GlassStore::find_glass_by_weight(units::Grams glass_weight) const
{
    static constexpr units::Grams kTolerance = 5 * units::si::gram;
    auto it = glass_weights_.lower_bound(glass_weight);
    auto best_it = glass_weights_.end();

    const auto update_best = [&](auto&& candidate) {
        if (candidate != glass_weights_.end()) {
            const auto diff = units::abs(candidate->first - glass_weight);
            if (diff <= kTolerance) {
                if (best_it == glass_weights_.end() or diff < units::abs(best_it->first - glass_weight)) {
                    best_it = candidate;
                }
            }
        }
    };

    // Check the element at lower_bound and the one before it,
    // because either could be the closest.
    update_best(it);
    if (it != glass_weights_.begin()) {
        update_best(std::prev(it));
    }

    if (best_it != glass_weights_.end()) {
        return glasses_.find(best_it->second)->second;
    }

    throw std::runtime_error("No glass found within tolerance"); // TODO: add own exceptions
}

const GlassStore::GlassMap& GlassStore::glasses() const
{
    return glasses_;
}
} // namespace cm
