module;
#include <simdjson.h>
#include <spdlog/spdlog.h>

module cm;
import std;
import mp_units;
import cm.core;
import :catalog_loader;

namespace cm {

void GlassStore::init_glasses(std::vector<Glass> glasses)
{
    std::unordered_map<GlassId, Glass> catalog;
    for (auto&& glass : glasses) {
        auto id = glass.id;
        if (not catalog.emplace(id, std::move(glass)).second) {
            throw std::invalid_argument{std::format("Duplicate glass id '{}'", id)};
        }
    }
    glasses_ = std::move(catalog);
    load_active_volumes();
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
            save_active_volumes();
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
            save_active_volumes();
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
    return load_json_catalog<Glass>(
        dir, "glass", [](simdjson::ondemand::document& document, const std::filesystem::path& /*file*/) {
            return parse_glass(document);
        });
}

namespace {
std::string serialize_active_volumes(const std::unordered_map<GlassId, Glass>& glasses)
{
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
        throw std::runtime_error{simdjson::error_message(error)};
    }
    return std::string{json};
}

void apply_active_volumes(const std::string& content,
                          const std::filesystem::path& path,
                          std::unordered_map<GlassId, Glass>& glasses,
                          const log::Logger& logger)
{
    const simdjson::padded_string json{content};
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
} // namespace

GlassStore::GlassStore(std::filesystem::path active_volumes_file)
    : writer_{std::move(active_volumes_file)}
{
}

void GlassStore::flush()
{
    writer_.flush();
}

void GlassStore::load_active_volumes()
{
    if (const auto content = read_state_file(writer_.file(), logger_); content.has_value()) {
        apply_active_volumes(*content, writer_.file(), glasses_, logger_);
    }
}

void GlassStore::save_active_volumes()
{
    try {
        writer_.save(serialize_active_volumes(glasses_));
    }
    catch (const std::exception& error) {
        SPDLOG_LOGGER_ERROR(logger_, "Could not serialize the active glass volumes: {}", error.what());
    }
}

} // namespace cm
