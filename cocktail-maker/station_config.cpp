module;
#include <simdjson.h>
#include <spdlog/spdlog.h>

module cm:station_config_impl;

import std;
import cm.core;
import :pod_types;
import :ingredient;
import :dispenser;
import :station_config;

namespace cm {

StationConfig::StationConfig(const IngredientStore& ingredient_store, std::filesystem::path db_file)
    : ingredient_store_{ingredient_store}
    , db_file_{std::move(db_file)}
{
}

void StationConfig::init()
{
    load_config_from_file();
}

void StationConfig::update_dispenser_ingredient_mapping(IngredientId ingredient_id, PodDispenser pod_dispenser_pair)
{
    SPDLOG_LOGGER_DEBUG(logger_,
                        "Pod '{}', Dispenser '{}' maps to ingredient '{}'",
                        pod_dispenser_pair.pod_id,
                        pod_dispenser_pair.dispenser_id,
                        ingredient_id);
    if (not ingredient_store_.find_by_id(ingredient_id).has_value()) {
        SPDLOG_LOGGER_ERROR(logger_, "Could not find ingredient '{}'.", ingredient_id);
        // question: throw exception here and bring it to the ui? Maybe with toasts?
        return;
    }
    // allow unknown pod->dispenser pair for now, as the config file will be loaded later on before anything is
    // discovered/connected.
    ingredient_dispenser_mapping_.insert_or_assign(std::move(ingredient_id), std::move(pod_dispenser_pair));
    save_config_to_file();
}

std::expected<PodDispenser, std::out_of_range> StationConfig::find_dispenser_for_ingredient(IngredientId ingredient_id) const
{
    auto it = ingredient_dispenser_mapping_.find(ingredient_id);
    if (it == ingredient_dispenser_mapping_.end()) {
        return std::unexpected{std::out_of_range{std::format("Could not find a dispenser for ingredient '{}'.", ingredient_id)}};
    }
    return it->second;
}

std::expected<IngredientId, std::out_of_range> StationConfig::find_ingredient_by_dispenser(PodDispenser pod_dispenser) const
{
    auto it =
        std::ranges::find_if(ingredient_dispenser_mapping_, [&](const auto& entry) { return entry.second == pod_dispenser; });

    if (it == ingredient_dispenser_mapping_.end()) {
        return std::unexpected(std::out_of_range("No ingredient found for the given pod/dispenser combination"));
    }

    return it->first;
}

std::filesystem::path StationConfig::mapping_file_path()
{
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        SPDLOG_ERROR("Could not determine current working directory: {}", ec.message());
        return "ingredient_dispenser_mapping.json";
    }
    return cwd / "ingredient_dispenser_mapping.json";
}

void StationConfig::save_config_to_file() const
{
    simdjson::builder::string_builder sb;
    sb.start_object();
    sb.append_key_value("version", 1);
    sb.append_comma();
    sb.escape_and_append_with_quotes("mappings");
    sb.append_colon();
    sb.start_array();

    bool first = true;
    for (const auto& [ingredient_id, pod_dispenser] : ingredient_dispenser_mapping_) {
        if (not first) {
            sb.append_comma();
        }
        first = false;

        sb.start_object();
        sb.append_key_value("ingredient_id", ingredient_id.raw());
        sb.append_comma();
        sb.append_key_value("pod_id", pod_dispenser.pod_id.raw());
        sb.append_comma();
        sb.append_key_value("dispenser_id", pod_dispenser.dispenser_id.raw());
        sb.end_object();
    }

    sb.end_array();
    sb.end_object();

    std::string_view json;
    if (auto error = sb.view().get(json)) {
        SPDLOG_LOGGER_ERROR(logger_, "Failed to serialize dispenser mapping: {}", simdjson::error_message(error));
        return;
    }

    const auto path = mapping_file_path();
    std::ofstream ofs{path, std::ios::binary | std::ios::trunc};
    if (not ofs) {
        SPDLOG_LOGGER_ERROR(logger_, "Could not open '{}' for writing.", path.string());
        return;
    }
    ofs << json;
    if (not ofs) {
        SPDLOG_LOGGER_ERROR(logger_, "Failed writing dispenser mapping to '{}'.", path.string());
        return;
    }

    SPDLOG_LOGGER_DEBUG(logger_, "Saved {} dispenser mapping(s) to '{}'.", ingredient_dispenser_mapping_.size(), path.string());
}

void StationConfig::load_config_from_file()
{
    const auto path = mapping_file_path();

    std::error_code ec;
    if (not std::filesystem::exists(path, ec) || ec) {
        SPDLOG_LOGGER_DEBUG(logger_, "No dispenser mapping config found at '{}', starting empty.", path.string());
        return;
    }

    simdjson::padded_string json;
    if (auto error = simdjson::padded_string::load(path.string()).get(json)) {
        SPDLOG_LOGGER_ERROR(logger_, "Failed to read '{}': {}", path.string(), simdjson::error_message(error));
        return;
    }

    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;
    if (auto error = parser.iterate(json).get(doc)) {
        SPDLOG_LOGGER_ERROR(logger_, "Failed to parse '{}': {}", path.string(), simdjson::error_message(error));
        return;
    }

    simdjson::ondemand::array mappings;
    if (auto error = doc["mappings"].get_array().get(mappings)) {
        SPDLOG_LOGGER_ERROR(logger_, "Malformed '{}': missing 'mappings' array.", path.string());
        return;
    }

    ingredient_dispenser_mapping_.clear();

    for (auto entry_result : mappings) {
        simdjson::ondemand::object entry;
        if (auto error = entry_result.get_object().get(entry)) {
            SPDLOG_LOGGER_ERROR(logger_, "Malformed entry in '{}', skipping.", path.string());
            continue;
        }

        std::string_view raw_ingredient_id;
        std::string_view raw_pod_id;
        std::int64_t raw_dispenser_id{};

        if (entry["ingredient_id"].get(raw_ingredient_id) or entry["pod_id"].get(raw_pod_id) or
            entry["dispenser_id"].get(raw_dispenser_id)) {
            SPDLOG_LOGGER_ERROR(logger_, "Malformed entry in '{}', skipping.", path.string());
            continue;
        }

        auto ingredient_id = IngredientId{raw_ingredient_id};

        if (not ingredient_store_.find_by_id(ingredient_id).has_value()) {
            SPDLOG_LOGGER_WARN(logger_, "Ingredient '{}' from config not found in ingredient store, skipping.", ingredient_id);
            continue;
        }

        ingredient_dispenser_mapping_.insert_or_assign(
            std::move(ingredient_id),
            PodDispenser{PodId{std::string{raw_pod_id}}, DispenserId{static_cast<int>(raw_dispenser_id)}});
    }

    SPDLOG_LOGGER_INFO(logger_, "Loaded {} dispenser mapping(s) from '{}'.", ingredient_dispenser_mapping_.size(), path.string());
}
} // namespace cm
