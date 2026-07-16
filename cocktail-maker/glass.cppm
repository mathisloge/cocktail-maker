module;
#include <mp-units/systems/si/units.h>
export module cm:glass;
import std;
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

export std::vector<Glass> load_glasses_from_dir(std::filesystem::path dir);

export class GlassStore
{
  public:
    void init_from_dir(std::filesystem::path dir);

    const std::unordered_map<GlassId, Glass>& glasses() const
    {
        return glasses_;
    }

  private:
    std::unordered_map<GlassId, Glass> glasses_;
};

void GlassStore::init_from_dir(std::filesystem::path dir)
{
    glasses_.clear();
    for (auto&& glass : load_glasses_from_dir(std::move(dir))) {
        auto id = glass.id;
        glasses_.emplace(std::move(id), std::move(glass));
    }
}

std::vector<Glass> load_glasses_from_dir(std::filesystem::path dir)
{
    // TODO: load from filesystem
    return {{
        .id = GlassId{"highball"},
        .display_name = "Highball Glas",
        .common_volumes =
            {
                300 * units::milli_litre,
                350 * units::milli_litre,
                400 * units::milli_litre,
            },
        .icon =
            GlassIconData{
                .viewbox_width = 100,
                .viewbox_height = 180,
                .outline = "M 22 18 L 22 158 Q 22 162 26 162 L 74 162 Q 78 162 78 158 L 78 18",
                .rim = {"M 18 18 L 82 18", "M 22 26 L 78 26"},
            },
    }};
}
} // namespace cm
