module;
#include <libassert/assert-macros.hpp>
export module cm:pod_registry;

import std;
import libassert;
import cm.core;
import :pod;
import :pod_types;
import :dispenser;

namespace cm {

namespace {

// Mirrors std::owner_equal (P1901R2) for any shared_ptr/weak_ptr combination.
// Drop-in removable once defines __cpp_lib_smart_ptr_owner_equality.
template <class A, class B>
    requires requires(const A& a, const B& b) {
        a.owner_before(b);
        b.owner_before(a);
    }
[[nodiscard]] bool owner_equal(const A& a, const B& b) noexcept
{
    return !a.owner_before(b) && !b.owner_before(a);
}

} // namespace

export class PodRegistry
{
  public:
    std::expected<std::unique_ptr<Dispenser>, std::out_of_range> dispenser_of_pod(PodId pod_id, DispenserId dispenser_id) const
    {
        for (const auto& weak_pod : pods_) {
            if (auto pod = weak_pod.lock()) {
                if (pod->pod_id() == pod_id) {
                    return pod->create_dispenser(dispenser_id).transform_error([](auto&& err) {
                        return std::out_of_range{err.what()};
                    });
                }
            }
        }
        return std::unexpected{std::out_of_range{"Pod not found"}};
    }

    void register_pod(std::shared_ptr<IPod> pod)
    {
        ASSERT(std::ranges::find_if(pods_, [&pod](const std::weak_ptr<IPod>& p) { return owner_equal(p, pod); }) == pods_.end(),
               "A pod should only be added one time.");
        pods_.emplace_back(std::move(pod));
    }

    void unregister_pod(std::shared_ptr<IPod> pod)
    {
        std::erase_if(pods_, [&pod](const std::weak_ptr<IPod>& p) { return owner_equal(p, pod); });
    }

  private:
    log::Logger logger_{log::create_or_get("pod_registry")};
    std::vector<std::weak_ptr<IPod>> pods_;
};
} // namespace cm
