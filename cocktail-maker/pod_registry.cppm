module;
#include <libassert/assert-macros.hpp>
export module cm:pod_registry;

import std;
import libassert;
import :logging;
import :pod;
import :pod_types;

namespace cm {
export class PodRegistry
{
  public:
    void register_pod(IPod& pod)
    {
        ASSERT((std::ranges::find(pods_, &pod) == pods_.end()), "A pod should only be added one time.");
        pods_.emplace_back(&pod);
    }

    void unregister_pod(IPod& pod)
    {
        std::erase(pods_, &pod);
    }

  private:
    log::Logger logger_{log::create_or_get("pod_registry")};
    std::vector<IPod*> pods_;
};
} // namespace cm
