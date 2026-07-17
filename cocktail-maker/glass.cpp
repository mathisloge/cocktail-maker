module;
#include <simdjson/builder.h>
#include <simdjson/ondemand.h>
#include <spdlog/spdlog.h>

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
    load_glass_active_volumes(glasses_);
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
            save_glass_active_volumes(glasses_);
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
            save_glass_active_volumes(glasses_);
            return kRemoved;
        }
    }
    return not kRemoved;
}

Glass parse_glass(simdjson::ondemand::document& doc)
{
    Glass glass;
    simdjson::ondemand::object obj = doc.get_object();

    // Parse Id & Display Name
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
    glass.icon.outline = icon_obj["outline"].get_string().value();

    // Rim is optional in the schema (default: [])
    simdjson::ondemand::array rim_arr;
    if (icon_obj["rim"].get(rim_arr) == simdjson::SUCCESS) {
        for (auto rim_str : rim_arr) {
            glass.icon.rim.emplace_back(rim_str.get_string().value());
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

log::Logger glass_volume_config_logger()
{
    static log::Logger logger{log::create_or_get("glass_volume_config")};
    return logger;
}

std::filesystem::path glass_volume_config_file_path()
{
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        SPDLOG_ERROR("Could not determine current working directory: {}", ec.message());
        return "glass_active_volumes.json";
    }
    return cwd / "glass_active_volumes.json";
}

void save_glass_active_volumes(const std::unordered_map<GlassId, Glass>& glasses)
{
    auto logger = glass_volume_config_logger();

    simdjson::builder::string_builder sb;
    sb.start_object();
    sb.append_key_value("version", 1);
    sb.append_comma();
    sb.escape_and_append_with_quotes("glasses");
    sb.append_colon();
    sb.start_array();

    bool first_glass = true;
    for (const auto& [glass_id, glass] : glasses) {
        if (not first_glass) {
            sb.append_comma();
        }
        first_glass = false;

        sb.start_object();
        sb.append_key_value("glass_id", glass_id.raw());
        sb.append_comma();
        sb.escape_and_append_with_quotes("active_volumes");
        sb.append_colon();
        sb.start_array();

        bool first_volume = true;
        for (const auto& volume : glass.active_volumes) {
            if (not first_volume) {
                sb.append_comma();
            }
            first_volume = false;
            sb.append(volume.numerical_value_in(units::milli_litre));
        }

        sb.end_array();
        sb.end_object();
    }

    sb.end_array();
    sb.end_object();

    std::string_view json;
    if (auto error = sb.view().get(json)) {
        SPDLOG_LOGGER_ERROR(logger, "Failed to serialize glass active volumes: {}", simdjson::error_message(error));
        return;
    }

    const auto path = glass_volume_config_file_path();
    std::ofstream ofs{path, std::ios::binary | std::ios::trunc};
    if (not ofs) {
        SPDLOG_LOGGER_ERROR(logger, "Could not open '{}' for writing.", path.string());
        return;
    }
    ofs << json;
    if (not ofs) {
        SPDLOG_LOGGER_ERROR(logger, "Failed writing glass active volumes to '{}'.", path.string());
        return;
    }

    SPDLOG_LOGGER_DEBUG(logger, "Saved active volumes for {} glass(es) to '{}'.", glasses.size(), path.string());
}

void load_glass_active_volumes(std::unordered_map<GlassId, Glass>& glasses)
{
    auto logger = glass_volume_config_logger();
    const auto path = glass_volume_config_file_path();

    std::error_code ec;
    if (not std::filesystem::exists(path, ec) || ec) {
        SPDLOG_LOGGER_DEBUG(logger, "No glass active volumes config found at '{}', leaving defaults.", path.string());
        return;
    }

    simdjson::padded_string json;
    if (auto error = simdjson::padded_string::load(path.string()).get(json)) {
        SPDLOG_LOGGER_ERROR(logger, "Failed to read '{}': {}", path.string(), simdjson::error_message(error));
        return;
    }

    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;
    if (auto error = parser.iterate(json).get(doc)) {
        SPDLOG_LOGGER_ERROR(logger, "Failed to parse '{}': {}", path.string(), simdjson::error_message(error));
        return;
    }

    simdjson::ondemand::array glasses_json;
    if (auto error = doc["glasses"].get_array().get(glasses_json)) {
        SPDLOG_LOGGER_ERROR(logger, "Malformed '{}': missing 'glasses' array.", path.string());
        return;
    }

    for (auto entry_result : glasses_json) {
        simdjson::ondemand::object entry;
        if (auto error = entry_result.get_object().get(entry)) {
            SPDLOG_LOGGER_ERROR(logger, "Malformed glass entry in '{}', skipping.", path.string());
            continue;
        }

        std::string_view raw_glass_id;
        if (entry["glass_id"].get(raw_glass_id)) {
            SPDLOG_LOGGER_ERROR(logger, "Malformed glass entry in '{}', skipping.", path.string());
            continue;
        }

        simdjson::ondemand::array volumes_json;
        if (entry["active_volumes"].get_array().get(volumes_json)) {
            SPDLOG_LOGGER_ERROR(logger, "Malformed glass entry in '{}', skipping.", path.string());
            continue;
        }

        auto glass_id = GlassId{raw_glass_id};
        auto it = glasses.find(glass_id);
        if (it == glasses.end()) {
            SPDLOG_LOGGER_WARN(logger, "Glass '{}' from config not found, skipping.", glass_id);
            continue;
        }

        std::vector<units::Litre> active_volumes;
        for (auto volume_result : volumes_json) {
            double raw_volume{};
            if (volume_result.get(raw_volume)) {
                SPDLOG_LOGGER_ERROR(
                    logger, "Malformed volume entry for glass '{}' in '{}', skipping value.", glass_id, path.string());
                continue;
            }
            active_volumes.push_back(raw_volume * units::milli_litre);
        }

        std::ranges::sort(active_volumes);
        active_volumes.erase(std::ranges::unique(active_volumes).begin(), active_volumes.end());
        it->second.active_volumes = std::move(active_volumes);
    }

    SPDLOG_LOGGER_INFO(logger, "Loaded active volumes from '{}'.", path.string());
}

} // namespace cm
