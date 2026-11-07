module;

export module cm:glass;
import std;
import cm.core;
import :state_file;

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

/// The glass catalog together with the volumes the operator switched on, which it keeps in `active_volumes_file`.
export class GlassStore
{
  public:
    explicit GlassStore(std::filesystem::path active_volumes_file);

    /// Replaces the catalog and reads the active volumes for it. @throws std::invalid_argument if two glasses share an id.
    void init_glasses(std::vector<Glass> glasses);

    [[nodiscard]] const std::unordered_map<GlassId, Glass>& glasses() const;

    bool add_active_volume(const GlassId& id, units::Litre volume);

    bool remove_active_volume(const GlassId& id, units::Litre volume);

    /// Blocks until every saved change has reached the file, so it must not run on the execution context.
    void flush();

  private:
    void load_active_volumes();
    void save_active_volumes();

    log::Logger logger_{log::create_or_get("glass_volume_config")};
    std::unordered_map<GlassId, Glass> glasses_;
    StateFileWriter writer_;
};
} // namespace cm
