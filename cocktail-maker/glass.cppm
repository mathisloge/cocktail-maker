module;

export module cm:glass;
import std;
import mp_units;
import cm.core;

namespace cm {

export using GlassId = strong_type<std::string, struct GlassIdTag, Comparable, Hashable, Formattable>;

export struct GlassIconData
{
    float viewbox_width;
    float viewbox_height;
    std::string outline;
    std::vector<std::string> rim;
};

export struct Glass
{
    GlassId id;
    std::string display_name;
    std::vector<units::Litre> common_volumes;
    std::vector<units::Litre> active_volumes;
    GlassIconData icon;
};
} // namespace cm
