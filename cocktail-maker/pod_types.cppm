module;
export module cm:pod_types;

import std;
import cm.core;

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
    Version protocol_version;
    int num_pumps;
    int num_valves;
};
} // namespace cm

namespace std {
template <>
struct formatter<cm::Version, char> : formatter<std::string_view>
{
    template <class FormatContext>
    auto format(const cm::Version& version, FormatContext& ctx) const
    {
        return std::format_to(ctx.out(), "{}.{}.{}", version.major, version.minor, version.patch);
    }
};
} // namespace std
