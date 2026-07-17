module;
#include <simdjson/builder.h>
#include <simdjson/ondemand.h>

module cm:glass_impl;
import std;
import mp_units;
import cm.core;
import :glass;

namespace cm {

void GlassStore::init_glasses(std::vector<Glass> glasses)
{
    glasses_.clear();
    for (auto&& glass : glasses) {
        auto id = glass.id;
        glasses_.emplace(std::move(id), std::move(glass));
    }
}

const std::unordered_map<GlassId, Glass>& GlassStore::glasses() const
{
    return glasses_;
}

bool GlassStore::add_active_volume(const GlassId& id, units::Litre volume)
{
    constexpr bool kAdded = true;
    if (auto it = glasses_.find(id); it != glasses_.end()) {
        auto& active_volumes = it->second.active_volumes;

        auto vol_it = std::ranges::lower_bound(active_volumes, volume);
        if (vol_it == active_volumes.end() || *vol_it != volume) {
            active_volumes.insert(vol_it, volume);
            return kAdded;
        }
    }
    return not kAdded;
}

bool GlassStore::remove_active_volume(const GlassId& id, units::Litre volume)
{
    constexpr bool kRemoved = true;
    if (auto it = glasses_.find(id); it != glasses_.end()) {
        auto& active_volumes = it->second.active_volumes;

        auto vol_it = std::ranges::find(active_volumes, volume);
        if (vol_it != active_volumes.end()) {
            active_volumes.erase(vol_it);
            return kRemoved;
        }
    }
    return not kRemoved;
}

Glass parse_glass(simdjson::ondemand::document& doc)
{
    Glass glass;
    simdjson::ondemand::object obj = doc.get_object();

    // 1. Parse Id & Display Name
    // Note: Assuming strong_type allows explicit initialization from std::string
    glass.id = GlassId{obj["id"].get_string().value()};
    glass.display_name = obj["display_name"].get_string().value();
    for (auto vol : obj["common_volumes_ml"].get_array()) {
        const int64_t ml = vol.get_int64().value();
        glass.common_volumes.push_back(ml * units::milli_litre);
    }
    // Parse Icon Data
    simdjson::ondemand::object icon_obj = obj["icon"].get_object();
    glass.icon.viewbox_width = static_cast<float>(icon_obj["viewbox_width"].get_double().value());
    glass.icon.viewbox_height = static_cast<float>(icon_obj["viewbox_height"].get_double().value());
    glass.icon.outline = std::string(icon_obj["outline"].get_string().value());

    // Rim is optional in the schema (default: [])
    simdjson::ondemand::array rim_arr;
    if (icon_obj["rim"].get(rim_arr) == simdjson::SUCCESS) {
        for (auto rim_str : rim_arr) {
            glass.icon.rim.emplace_back(std::string(rim_str.get_string().value()));
        }
    }

    return glass;
}

std::vector<Glass> load_glasses_from_dir(std::filesystem::path dir)
{
    std::vector<Glass> glasses;
    simdjson::ondemand::parser parser;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            try {
                simdjson::padded_string json = simdjson::padded_string::load(entry.path().string());
                simdjson::ondemand::document doc = parser.iterate(json);

                glasses.push_back(parse_glass(doc));
            }
            catch (const simdjson::simdjson_error& e) {
                throw std::runtime_error(std::format("Failed to parse {}: {}", entry.path().string(), e.what()));
            }
        }
    }
    return glasses;
}
} // namespace cm
