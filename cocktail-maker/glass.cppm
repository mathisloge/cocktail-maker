module;

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
export void save_glass_active_volumes(const std::unordered_map<GlassId, Glass>& glasses);
export void load_glass_active_volumes(std::unordered_map<GlassId, Glass>& glasses);

export class GlassStore
{
  public:
    void init_glasses(std::vector<Glass> glasses);

    [[nodiscard]] const std::unordered_map<GlassId, Glass>& glasses() const;

    bool add_active_volume(const GlassId& id, units::Litre volume);

    bool remove_active_volume(const GlassId& id, units::Litre volume);

  private:
    std::unordered_map<GlassId, Glass> glasses_;
};
} // namespace cm
