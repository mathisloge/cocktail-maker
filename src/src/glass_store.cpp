#include "cm/glass_store.hpp"

namespace cm {
const Glass& GlassStore::add_glass(Glass glass, units::Grams glass_weight)
{
    auto&& [it, _] = glasses_.emplace(glass_weight, std::move(glass));
    return it->second;
}

const Glass& GlassStore::find_glass_by_weight(units::Grams glass_weight)
{
    static constexpr units::Grams kTolerance = 5 * units::si::gram;
    auto it = glasses_.lower_bound(glass_weight);
    auto best_it = glasses_.end();

    const auto update_best = [&](WeightGlassMap::iterator candidate) {
        if (candidate != glasses_.end()) {
            const auto diff = units::abs(candidate->first - glass_weight);
            if (diff <= kTolerance) {
                if (best_it == glasses_.end() or diff < units::abs(best_it->first - glass_weight)) {
                    best_it = candidate;
                }
            }
        }
    };

    // Check the element at lower_bound and the one before it,
    // because either could be the closest.
    update_best(it);
    if (it != glasses_.begin()) {
        update_best(std::prev(it));
    }

    if (best_it != glasses_.end()) {
        return best_it->second;
    }

    throw std::runtime_error("No glass found within tolerance"); // TODO: add own exceptions
}
} // namespace cm
