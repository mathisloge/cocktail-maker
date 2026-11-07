module;
#include <boost/asio/any_io_executor.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm:fill_level_impl;

import std;
import mp_units;
import cm.core;
import :pod;
import :fill_level;

namespace asio = boost::asio;

namespace cm {

namespace {
double in_grams(const units::Grams mass)
{
    return mass.numerical_value_in(units::si::gram);
}

units::Grams to_mass(const units::Litre volume)
{
    return volume * kAssumedDensity;
}

/// What each ingredient adds up to over all steps, in the order of the recipe.
std::vector<std::pair<IngredientId, units::Litre>> amounts_per_ingredient(const Commands& commands)
{
    std::vector<std::pair<IngredientId, units::Litre>> amounts;
    const auto add = [&amounts](const Command& command) {
        const auto* pour = std::get_if<DispenseCommand>(&command);
        if (pour == nullptr) {
            return;
        }
        const auto it = std::ranges::find(amounts, pour->ingredient, [](const auto& entry) { return entry.first; });
        if (it == amounts.end()) {
            amounts.emplace_back(pour->ingredient, pour->volume);
        }
        else {
            it->second += pour->volume;
        }
    };

    for (const auto& step : commands) {
        std::visit(detail::Overloaded{[&add](const Command& command) { add(command); },
                                      [&add](const ParallelCommand& group) { std::ranges::for_each(group, add); }},
                   step);
    }
    return amounts;
}

std::string serialize(const std::map<PodDispenser, FillLevelRecord>& records)
{
    simdjson::builder::string_builder sb;
    sb.start_object();
    sb.append_key_value("version", 1);
    sb.append_comma();
    sb.escape_and_append_with_quotes("fill_levels");
    sb.append_colon();
    sb.start_array();

    bool first = true;
    for (const auto& [address, record] : records) {
        if (not first) {
            sb.append_comma();
        }
        first = false;

        sb.start_object();
        sb.append_key_value("pod_id", address.pod_id.raw());
        sb.append_comma();
        sb.append_key_value("dispenser_id", address.dispenser_id.raw());
        sb.append_comma();
        sb.append_key_value("level_g", in_grams(record.level));
        if (record.capacity.has_value()) {
            sb.append_comma();
            sb.append_key_value("capacity_g", in_grams(*record.capacity));
        }
        sb.append_comma();
        sb.append_key_value("low_water_g", in_grams(record.low_water));
        sb.append_comma();
        sb.append_key_value("suspected_empty", record.suspected_empty);
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

} // namespace

units::Litre to_volume(const units::Grams mass)
{
    return mass / kAssumedDensity;
}

FillLevelTracker::FillLevelTracker(asio::any_io_executor executor, std::filesystem::path file, FillLevelOptions options)
    : executor_{std::move(executor)}
    , options_{std::move(options)}
    , writer_{std::move(file)}
{
}

void FillLevelTracker::init()
{
    const auto& file = writer_.file();
    const auto content = read_state_file(file, logger_);
    if (not content.has_value()) {
        return;
    }
    const simdjson::padded_string json{*content};

    simdjson::dom::parser parser;
    simdjson::dom::element document;
    if (auto error = parser.parse(json).get(document)) {
        SPDLOG_LOGGER_ERROR(logger_, "Failed to parse '{}': {}", file.string(), simdjson::error_message(error));
        return;
    }

    simdjson::dom::array entries;
    if (auto error = document["fill_levels"].get(entries)) {
        SPDLOG_LOGGER_ERROR(logger_, "Malformed '{}': missing 'fill_levels' array.", file.string());
        return;
    }

    records_.clear();
    corrections_.clear();
    for (const simdjson::dom::element entry : entries) {
        std::string_view pod_id;
        std::int64_t dispenser_id{};
        double level{};
        double low_water{};
        bool suspected_empty{};
        if (entry["pod_id"].get(pod_id) or entry["dispenser_id"].get(dispenser_id) or entry["level_g"].get(level) or
            entry["low_water_g"].get(low_water) or entry["suspected_empty"].get(suspected_empty)) {
            SPDLOG_LOGGER_ERROR(logger_, "Malformed entry in '{}', skipping.", file.string());
            continue;
        }

        FillLevelRecord record{.level = level * units::si::gram,
                               .capacity = std::nullopt,
                               .low_water = low_water * units::si::gram,
                               .suspected_empty = suspected_empty};
        double capacity{};
        if (not entry["capacity_g"].get(capacity)) {
            record.capacity = capacity * units::si::gram;
        }
        records_.insert_or_assign(PodDispenser{PodId{std::string{pod_id}}, DispenserId{static_cast<int>(dispenser_id)}},
                                  std::move(record));
    }

    SPDLOG_LOGGER_INFO(logger_, "Loaded {} fill level(s) from '{}'.", records_.size(), file.string());
}

void FillLevelTracker::flush()
{
    writer_.flush();
}

void FillLevelTracker::set_change_handler(ChangeHandler handler)
{
    change_handler_ = std::move(handler);
}

std::optional<FillLevelRecord> FillLevelTracker::record(const PodDispenser& address) const
{
    const auto it = records_.find(address);
    if (it == records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

FillLevelView FillLevelTracker::view(const PodDispenser& address) const
{
    const bool no_load_cell = without_load_cell_.contains(address);
    const auto found = record(address);
    if (not found.has_value()) {
        return FillLevelView{.no_load_cell = no_load_cell};
    }

    FillLevelView view{.level = std::max(found->level, 0.0 * units::si::gram),
                       .percent = std::nullopt,
                       .suspected_empty = found->suspected_empty,
                       .no_load_cell = no_load_cell};
    if (found->capacity.has_value() && *found->capacity > 0.0 * units::si::gram) {
        view.percent = in_grams(*view.level) / in_grams(*found->capacity) * 100.0 * units::percent;
    }
    return view;
}

Task<void> FillLevelTracker::correct_on_connect(const PodDispenser& address, Dispenser& dispenser)
{
    corrections_.erase(address);
    if (const auto reading = co_await read_weight(address, dispenser)) {
        apply_reading(address, *reading);
    }
    changed(address);
}

void FillLevelTracker::pod_disconnected(const PodId& pod_id)
{
    std::erase_if(corrections_, [&pod_id](const auto& entry) { return entry.first.pod_id == pod_id; });
    std::erase_if(without_load_cell_, [&pod_id](const PodDispenser& address) { return address.pod_id == pod_id; });
}

Task<void> FillLevelTracker::refresh(const PodDispenser& address, Dispenser& dispenser)
{
    if (not records_.contains(address)) {
        co_return;
    }
    if (const auto reading = co_await read_weight(address, dispenser)) {
        apply_reading(address, *reading);
        changed(address);
    }
}

Task<void> FillLevelTracker::pour_finished(const PodDispenser& address, Dispenser& dispenser, const DispenseReport& report)
{
    if (not records_.contains(address)) {
        co_return;
    }
    const auto reading = co_await read_weight(address, dispenser);

    const auto it = records_.find(address);
    if (it == records_.end()) {
        co_return;
    }
    auto& record = it->second;
    // Without a reading or a correction, the reading can't say what this pour took.
    if (not reading.has_value() || not corrections_.contains(address)) {
        record.level -= to_mass(report.amount);
    }
    if (reading.has_value()) {
        apply_reading(address, *reading);
    }

    const bool was_suspected = record.suspected_empty;
    record.suspected_empty = report.outcome == DispenseOutcome::bottle_empty;
    if (not record.suspected_empty) {
        record.low_water = std::min(record.low_water, record.level);
    }
    else if (not was_suspected || report.amount > 0.0 * units::milli_litre) {
        // Whatever the level claims, this is as low as the bottle gets. A refill counts from here. A dry pour on a bottle
        // already suspected says nothing new, and the bottle may be off the scale by now.
        record.low_water = record.level;
    }
    changed(address);
}

void FillLevelTracker::clear_scale_tared(const PodDispenser& address)
{
    records_.erase(address);
    corrections_.erase(address);
    changed(address);
}

void FillLevelTracker::empty_bottle_tared(const PodDispenser& address)
{
    records_.insert_or_assign(address,
                              FillLevelRecord{.level = 0.0 * units::si::gram,
                                              .capacity = std::nullopt,
                                              .low_water = 0.0 * units::si::gram,
                                              .suspected_empty = false});
    corrections_.insert_or_assign(address, 0.0 * units::si::gram);
    changed(address);
}

Task<void> FillLevelTracker::refilled(const PodDispenser& address, Dispenser& dispenser, const RefillSource source)
{
    co_await async_sleep(executor_, options_.refill_settle_time);

    const auto reading = co_await read_weight(address, dispenser);
    const auto it = records_.find(address);
    if (not reading.has_value() || it == records_.end()) {
        co_return;
    }
    auto& record = it->second;
    apply_reading(address, *reading);

    const units::Grams rise = record.level - record.low_water;
    if (rise >= options_.refill_min_rise) {
        if (record.suspected_empty) {
            record.level = rise;
            corrections_.insert_or_assign(address, record.level - *reading);
        }
        record.capacity = record.level;
    }
    else if (source == RefillSource::admin_area && record.capacity.has_value()) {
        // The scale shows nothing, yet an authenticated operator says it's full. The correction at connect has absorbed a
        // refill while the Station was off.
        record.level = *record.capacity;
        corrections_.insert_or_assign(address, record.level - *reading);
    }
    else {
        changed(address);
        co_return;
    }
    record.low_water = record.level;
    record.suspected_empty = false;
    changed(address);
}

Task<std::optional<units::Grams>> FillLevelTracker::read_weight(const PodDispenser& address, Dispenser& dispenser)
{
    try {
        const units::Grams reading = co_await dispenser.load_cell_weight();
        without_load_cell_.erase(address);
        co_return reading;
    }
    catch (const PodReceiveError& error) {
        if (error.error_code() == proto::field::ErrorCodeCommon::ValueType::NoLoadCell) {
            // That is normal for some dispensers.
            without_load_cell_.insert(address);
        }
        else {
            SPDLOG_LOGGER_WARN(logger_, "No weight available: {}", error.what());
        }
    }
    catch (const std::exception& error) {
        SPDLOG_LOGGER_WARN(logger_, "Could not read a weight: {}", error.what());
    }
    co_return std::nullopt;
}

void FillLevelTracker::apply_reading(const PodDispenser& address, const units::Grams reading)
{
    const auto it = records_.find(address);
    if (it == records_.end()) {
        return;
    }
    auto& record = it->second;

    const auto [correction, inserted] = corrections_.try_emplace(address, record.level - reading);
    record.level = reading + correction->second;
    // A suspected bottle may be off the scale, and that must not count as room for the next refill.
    if (not record.suspected_empty) {
        record.low_water = std::min(record.low_water, record.level);
    }
    if (record.capacity.has_value()) {
        record.capacity = std::max(*record.capacity, record.level);
    }
}

void FillLevelTracker::changed(const PodDispenser& address)
{
    if (change_handler_) {
        change_handler_(address, view(address));
    }

    try {
        writer_.save(serialize(records_));
    }
    catch (const std::exception& error) {
        SPDLOG_LOGGER_ERROR(logger_, "Could not serialize the fill levels: {}", error.what());
    }
}

std::vector<PodDispenser> involved_dispensers(const Commands& commands, const StationConfig& config)
{
    std::vector<PodDispenser> dispensers;
    for (const auto& [ingredient, amount] : amounts_per_ingredient(commands)) {
        const auto address = config.find_dispenser_for_ingredient(ingredient);
        if (address.has_value() && not std::ranges::contains(dispensers, *address)) {
            dispensers.push_back(*address);
        }
    }
    return dispensers;
}

std::vector<FillShortage> find_fill_shortages(const Commands& commands,
                                              const StationConfig& config,
                                              const FillLevelTracker& fill_levels)
{
    const auto whole_millilitres = [](const units::Litre volume) {
        return std::lround(volume.numerical_value_in(units::milli_litre));
    };

    std::vector<FillShortage> shortages;
    for (const auto& [ingredient, amount] : amounts_per_ingredient(commands)) {
        const auto address = config.find_dispenser_for_ingredient(ingredient);
        if (not address.has_value()) {
            continue;
        }
        const auto view = fill_levels.view(*address);
        if (not view.level.has_value()) {
            continue;
        }
        const units::Litre available = view.suspected_empty ? units::Litre{} : to_volume(*view.level);
        if (whole_millilitres(amount + kFillLevelReserve) > whole_millilitres(available)) {
            shortages.push_back(FillShortage{.ingredient = ingredient, .available = available, .needed = amount});
        }
    }
    return shortages;
}

Task<void> sync_pod_fill_levels(const PodInfo info, const PodRegistry& registry, FillLevelTracker& fill_levels)
{
    const int dispenser_count = info.num_pumps + info.num_valves;
    for (int index = 0; index < dispenser_count; ++index) {
        const DispenserId dispenser_id{index};
        auto dispenser = registry.dispenser_of_pod(info.id, dispenser_id);
        if (not dispenser.has_value()) {
            continue;
        }
        co_await fill_levels.correct_on_connect(PodDispenser{info.id, dispenser_id}, **dispenser);
    }
}
} // namespace cm
