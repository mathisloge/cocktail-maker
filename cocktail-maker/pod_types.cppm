module;
export module cm:pod_types;

import std;
import :strong_type;

namespace cm {
export using PodId = strong_type<std::string, struct PodIdTag, Comparable, Hashable, Formattable>;

export struct Version
{
    int major{};
    int minor{};
    int patch{};

    friend constexpr auto operator<=>(const Version&, const Version&) = default;
};

export struct PodInfo
{
    PodId id;
    Version firmware_version;
    int num_pumps;
    int num_valves;
};
} // namespace cm
