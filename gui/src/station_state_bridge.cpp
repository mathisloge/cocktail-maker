module;
#include <slint.h>
#include <spdlog/spdlog.h>
#include <time.h>
#include "app-window.h"

module cm.gui;

import std;
import mp_units;
import cm.core;
import cm;

namespace cm::gui {

Task<void> async_highlight_dispenser(const PodRegistry& pod_registry, const PodId pod_id, const DispenserId dispenser_id);
Task<void> async_mark_dispenser_refilled(const PodRegistry& pod_registry,
                                         FillLevelTracker& fill_levels,
                                         const PodId pod_id,
                                         const DispenserId dispenser_id);
Task<void> async_emergency_stop_all(const PodRegistry& pod_registry);

namespace {
constexpr std::string_view kUnknownFillText{"Füllstand unbekannt"};
constexpr double kAlmostEmptyPercent{15.0};
} // namespace

FillPresentation present_fill_level(const FillLevelView& view)
{
    if (view.no_load_cell) {
        return {.text = std::string{kUnknownFillText}};
    }
    if (view.suspected_empty) {
        return {.text = "vermutlich leer", .known = true, .ratio = 0.0F, .almost_empty = true};
    }
    if (not view.level.has_value()) {
        return {.text = std::string{kUnknownFillText}};
    }
    const double millilitres = to_volume(*view.level).numerical_value_in(units::milli_litre);
    if (not view.percent.has_value()) {
        return {.text = std::format("ca. {} mL", std::lround(millilitres))};
    }
    const double percent = view.percent->numerical_value_in(units::percent);
    // Without a share left the bottle's content cannot be derived from the level.
    if (percent <= 0.0) {
        return {.text = std::format("{} mL", std::lround(millilitres)), .known = true, .ratio = 0.0F, .almost_empty = true};
    }
    const double content = millilitres / (percent / 100.0);
    return {
        .text = std::format("{} mL von {} mL", std::lround(millilitres), std::lround(content)),
        .known = true,
        .ratio = static_cast<float>(std::clamp(percent / 100.0, 0.0, 1.0)),
        .almost_empty = percent < kAlmostEmptyPercent,
    };
}

std::string dispenser_label(const DispenserType type, const int number)
{
    return std::format("{} {}", type == DispenserType::Pump ? "Pumpe" : "Ventil", number);
}

using DispenserNames = std::map<PodDispenser, std::string>;

struct PodStateData
{
    PodInfo info;
    gui::ConnectionState connection_state{};
};

class PodStateImpl final : public PodState
{
  public:
    PodStateImpl(std::weak_ptr<StationStateBridge> parent)
        : parent_{std::move(parent)}
    {
    }

    PodStateImpl(PodStateImpl&&) noexcept = delete ("Instances are tracked and pointer stability is needed.");
    PodStateImpl& operator=(PodStateImpl&&) noexcept = delete;
    PodStateImpl(const PodStateImpl& other) = delete;
    PodStateImpl& operator=(const PodStateImpl&) = delete;

    ~PodStateImpl() override;

    const PodStateData& data() const
    {
        return data_;
    }

    void update_info(PodInfo info) override;

    void update_state(ConnectionState state) override;

  private:
    std::weak_ptr<StationStateBridge> parent_;
    PodStateData data_;
};

std::string pod_status_text(const PodStateData& pod)
{
    if (pod.connection_state != gui::ConnectionState::Incompatible) {
        return {};
    }
    return std::format("Protokoll {}, erwartet {}", pod.info.protocol_version, station_protocol_version());
}

class PodUiModel : public slint::Model<gui::Pod>
{
  private:
    std::optional<std::pair<size_t, gui::Pod*>> find_gui_pod(const PodStateData& pod)
    {
        auto it = std::ranges::find_if(pods_, [&](auto&& ui_pod) { return ui_pod.id == pod.info.id.raw().c_str(); });
        if (it == pods_.end()) {
            return std::nullopt;
        }
        auto row = static_cast<size_t>(std::distance(pods_.begin(), it));
        return std::pair{row, &(*it)};
    }

  public:
    size_t row_count() const override
    {
        return pods_.size();
    }

    std::optional<gui::Pod> row_data(size_t i) const override
    {
        if (i >= pods_.size()) {
            return std::nullopt;
        }
        return pods_.at(i);
    }

    void update_pod(const PodStateData& pod)
    {
        const auto apply_state_to_ui = [this](gui::Pod& gui_pod, const PodStateData& state_pod) {
            bool changed = false;
            if (gui_pod.id.data() != state_pod.info.id.raw()) {
                gui_pod.id = slint::SharedString{state_pod.info.id.raw()};
                changed = true;
            }
            if (gui_pod.connection_state != state_pod.connection_state) {
                gui_pod.connection_state = state_pod.connection_state;
                changed = true;
            }
            if (const auto status = pod_status_text(state_pod); gui_pod.status_text.data() != status) {
                gui_pod.status_text = slint::SharedString{status};
                changed = true;
            }
            if (const auto firmware = std::format("{}", state_pod.info.firmware_version);
                gui_pod.firmware_version.data() != firmware) {
                gui_pod.firmware_version = slint::SharedString{firmware};
                changed = true;
            }

            // An incompatible Pod offers nothing to assign, calibrate or identify, because it stays unused.
            const bool usable = state_pod.connection_state != gui::ConnectionState::Incompatible;
            const int num_pumps = usable ? state_pod.info.num_pumps : 0;
            const int num_valves = usable ? state_pod.info.num_valves : 0;
            if ((gui_pod.dispensers == nullptr) or gui_pod.dispensers->row_count() != (num_pumps + num_valves)) {
                auto dispenser_model = std::make_shared<slint::VectorModel<gui::Dispenser>>();
                const auto unknown = present_fill_level(FillLevelView{});
                const auto add = [&](int id, DispenserType type, int number) {
                    dispenser_model->push_back(gui::Dispenser{
                        .id = id,
                        .type = type,
                        .label = slint::SharedString{dispenser_label(type, number)},
                        .ingredient_name = slint::SharedString{name_of(state_pod.info.id, id)},
                        .fill_text = slint::SharedString{unknown.text},
                        .fill_known = unknown.known,
                        .fill_ratio = unknown.ratio,
                        .almost_empty = unknown.almost_empty,
                    });
                };
                for (int i = 0; i < num_pumps; ++i) {
                    add(i, DispenserType::Pump, i + 1);
                }
                for (int i = 0; i < num_valves; ++i) {
                    add(num_pumps + i, DispenserType::Valve, i + 1);
                }
                gui_pod.dispensers = std::move(dispenser_model);
                changed = true;
            }
            return changed;
        };
        if (auto existing_pod = find_gui_pod(pod); existing_pod.has_value()) {
            auto [row, gui_pod] = existing_pod.value();
            if (apply_state_to_ui(*gui_pod, pod)) {
                this->notify_row_changed(row);
                renumber_tiles();
            }
        }
        else {
            // Don't add pod when no id was assigned yet (discovery phase)
            if (pod.info.id == PodId{}) {
                return;
            }

            auto row = pods_.size();
            auto& inserted = pods_.emplace_back(gui::Pod{});

            apply_state_to_ui(inserted, pod);
            inserted.tile_offset = tile_count(row);
            this->notify_row_added(row, 1);
        }
    }

    void update_dispenser_fill(const PodDispenser& address, const FillPresentation& fill)
    {
        const auto pod_it =
            std::ranges::find_if(pods_, [&address](const auto& ui_pod) { return ui_pod.id == address.pod_id.raw().c_str(); });
        if (pod_it == pods_.end() || pod_it->dispensers == nullptr) {
            return;
        }
        for (std::size_t row = 0; row < pod_it->dispensers->row_count(); ++row) {
            auto dispenser = pod_it->dispensers->row_data(row);
            if (dispenser.has_value() && dispenser->id == address.dispenser_id.raw()) {
                dispenser->fill_text = slint::SharedString{fill.text};
                dispenser->fill_known = fill.known;
                dispenser->fill_ratio = fill.ratio;
                dispenser->almost_empty = fill.almost_empty;
                pod_it->dispensers->set_row_data(row, *dispenser);
                return;
            }
        }
    }

    void set_dispenser_names(DispenserNames names)
    {
        names_ = std::move(names);
        for (const auto& gui_pod : pods_) {
            if (gui_pod.dispensers == nullptr) {
                continue;
            }
            const PodId pod_id{std::string{gui_pod.id.data()}};
            for (std::size_t row = 0; row < gui_pod.dispensers->row_count(); ++row) {
                auto dispenser = gui_pod.dispensers->row_data(row);
                if (not dispenser.has_value()) {
                    continue;
                }
                if (const auto name = name_of(pod_id, dispenser->id); dispenser->ingredient_name.data() != name) {
                    dispenser->ingredient_name = slint::SharedString{name};
                    gui_pod.dispensers->set_row_data(row, *dispenser);
                }
            }
        }
    }

    void remove_pod(const PodStateData& pod)
    {
        const auto old_size = pods_.size();

        const auto [first, last] =
            std::ranges::remove_if(pods_, [&pod](const auto& ui_pod) { return ui_pod.id == pod.info.id.raw().c_str(); });

        const auto removed_cout = static_cast<size_t>(std::distance(first, last));
        if (removed_cout == 0) {
            return;
        }

        const auto index = static_cast<size_t>(std::distance(pods_.begin(), first));
        pods_.erase(first, last);
        notify_row_removed(index, removed_cout);
        renumber_tiles();
    }

  private:
    /// The number of dispensers of the rows before `end`.
    [[nodiscard]] int tile_count(std::size_t end) const
    {
        int count = 0;
        for (std::size_t row = 0; row < end; ++row) {
            count += pods_[row].dispensers == nullptr ? 0 : static_cast<int>(pods_[row].dispensers->row_count());
        }
        return count;
    }

    void renumber_tiles()
    {
        for (std::size_t row = 0; row < pods_.size(); ++row) {
            if (const int offset = tile_count(row); pods_[row].tile_offset != offset) {
                pods_[row].tile_offset = offset;
                notify_row_changed(row);
            }
        }
    }

    [[nodiscard]] std::string name_of(const PodId& pod_id, int dispenser_id) const
    {
        const auto it = names_.find(PodDispenser{pod_id, DispenserId{dispenser_id}});
        return it == names_.end() ? std::string{} : it->second;
    }

    std::vector<gui::Pod> pods_;
    DispenserNames names_;
};

constexpr std::string kSalt = "static";

StationStateBridge::StationStateBridge(slint::ComponentHandle<AppWindow> ui,
                                       const PodRegistry& pod_registry,
                                       AsyncScope& async_scope,
                                       FillLevelTracker& fill_levels)
    : ui_{std::move(ui)}
    , pod_model_{std::make_shared<PodUiModel>()}
    , operator_auth_{OperatorAuthConfig{
          .pin_hash = hash_pin("000000", kSalt),
          .salt = kSalt,
          .max_attempts = 3,
          .lockout_duration = std::chrono::seconds{10},
      }}
{
    ui_->global<StationStateContext>().set_pods(pod_model_);
    ui_->global<StationStateContext>().on_highlight_dispenser([&pod_registry, &async_scope](const gui::Pod pod,
                                                                                            const gui::Dispenser dispenser) {
        async_scope.spawn(async_highlight_dispenser(pod_registry, PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}));
    });
    ui_->global<StationStateContext>().on_mark_dispenser_refilled(
        [&pod_registry, &async_scope, &fill_levels](const gui::Pod pod, const gui::Dispenser dispenser) {
            async_scope.spawn(async_mark_dispenser_refilled(
                pod_registry, fill_levels, PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}));
        });
    fill_levels.set_change_handler([model = pod_model_](const PodDispenser& address, const FillLevelView& view) {
        slint::invoke_from_event_loop(
            [model, address, fill = present_fill_level(view)]() { model->update_dispenser_fill(address, fill); });
    });
    ui_->global<StationStateContext>().on_emergency_stop_all(
        [&pod_registry, &async_scope]() { async_scope.spawn(async_emergency_stop_all(pod_registry)); });
    ui_->global<StationStateContext>().on_clean_dispenser([this](const gui::Pod&, const gui::Dispenser&) {
        SPDLOG_LOGGER_WARN(logger_, "Cleaning a dispenser is not supported yet.");
    });
    ui_->global<StationStateContext>().on_reverse_dispenser([this](const gui::Pod&, const gui::Dispenser&) {
        SPDLOG_LOGGER_WARN(logger_, "Running a dispenser backwards is not supported yet.");
    });
    ui_->global<StationStateContext>().on_restart_pod(
        [this](const gui::Pod&) { SPDLOG_LOGGER_WARN(logger_, "Restarting a pod is not supported yet."); });
    ui_->global<StationStateContext>().on_reconnect_pod(
        [this](const gui::Pod&) { SPDLOG_LOGGER_WARN(logger_, "Reconnecting a pod is not supported yet."); });
    ui_->global<StationStateContext>().on_navigate_to([this](Page target) { on_navigate_to(target); });
    ui_->global<StationStateContext>().on_submit_operator_pin([this](slint::SharedString pin) { on_submit_operator_pin(pin); });
}

std::unique_ptr<PodState> StationStateBridge::create_pod_state()
{
    auto pod_state = std::make_unique<PodStateImpl>(shared_from_this());
    pods_.emplace_back(pod_state.get());
    return pod_state;
}

void StationStateBridge::watch_assignments(StationConfig& station_config, const IngredientStore& ingredient_store)
{
    const auto publish = [model = pod_model_, &station_config, &ingredient_store]() {
        DispenserNames names;
        for (const auto& [ingredient_id, address] : station_config.assignments()) {
            if (const auto ingredient = ingredient_store.find_by_id(ingredient_id); ingredient.has_value()) {
                names.insert_or_assign(address, ingredient->display_name);
            }
        }
        slint::invoke_from_event_loop(
            [model, names = std::move(names)]() mutable { model->set_dispenser_names(std::move(names)); });
    };
    station_config.set_change_handler(publish);
    publish();
}

void StationStateBridge::pod_connection_state_changed(const PodStateImpl& pod)
{
    update_pod_model(pod);
    track_problem(pod, false);
    const auto is_connected = [](const auto* p) { return p->data().connection_state == gui::ConnectionState::Connected; };
    // An incompatible Pod stays unused like an unplugged one, so it doesn't hold back the others.
    const auto is_settled = [&is_connected](const auto* p) {
        return is_connected(p) or p->data().connection_state == gui::ConnectionState::Incompatible;
    };
    const bool ready = std::ranges::any_of(pods_, is_connected) and std::ranges::all_of(pods_, is_settled);

    slint::invoke_from_event_loop([ready, ui = ui_]() { ui->global<cm::gui::StationStateContext>().set_station_ready(ready); });
}

void StationStateBridge::pod_about_to_removed(const PodStateImpl& pod)
{
    std::erase(pods_, &pod);
    pod_connection_state_changed(pod);
    track_problem(pod, true);
    // ^^ always remove pod after connection state re-eval, since otherwise it will be added to the model again.
    slint::invoke_from_event_loop([model = pod_model_, pod = pod.data()]() { model->remove_pod(pod); });
}

void StationStateBridge::pod_info_changed(const PodStateImpl& pod)
{
    if (auto p = find_pod(&pod); p.has_value()) {
        update_pod_model(*p.value());
    }
}

namespace {
std::string local_clock_time(const std::chrono::system_clock::time_point now)
{
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    // libc++ ships no time zone database here, so the C library converts to local time.
    if (::localtime_r(&seconds, &local) == nullptr) {
        return std::format("{:%H:%M}", std::chrono::floor<std::chrono::minutes>(now));
    }
    return std::format("{:02}:{:02}", local.tm_hour, local.tm_min);
}
} // namespace

void StationStateBridge::track_problem(const PodStateImpl& pod, const bool removed)
{
    const auto& data = pod.data();
    const auto& id = data.info.id.raw();
    if (id.empty()) {
        return;
    }
    if (removed) {
        problems_.insert_or_assign(id, std::format("{} getrennt um {}", id, local_clock_time(std::chrono::system_clock::now())));
    }
    else if (data.connection_state == gui::ConnectionState::Connected) {
        if (problems_.erase(id) == 0) {
            return;
        }
    }
    else if (data.connection_state == gui::ConnectionState::Incompatible) {
        problems_.insert_or_assign(id, std::format("{} hat eine inkompatible Firmware ({})", id, pod_status_text(data)));
    }
    else {
        return;
    }
    publish_problems();
}

void StationStateBridge::publish_problems()
{
    std::vector<gui::PodProblem> rows;
    for (const auto& [id, text] : problems_) {
        rows.push_back(gui::PodProblem{.pod_id = slint::SharedString{id}, .text = slint::SharedString{text}});
    }
    slint::invoke_from_event_loop([ui = ui_, rows = std::move(rows)]() {
        ui->global<StationStateContext>().set_pod_problems(std::make_shared<slint::VectorModel<gui::PodProblem>>(rows));
    });
}

std::shared_ptr<slint::Model<gui::Pod>> StationStateBridge::pod_model() const
{
    return pod_model_;
}

std::optional<PodStateImpl*> StationStateBridge::find_pod(const PodStateImpl* pod)
{
    if (pod == nullptr) {
        return std::nullopt;
    }
    auto it = std::ranges::find(pods_, pod);
    if (it == pods_.end()) {
        return std::nullopt;
    }
    return *it;
}

void StationStateBridge::update_pod_model(const PodStateImpl& pod)
{
    slint::invoke_from_event_loop([self = shared_from_this(), pod = pod.data()]() { self->pod_model_->update_pod(pod); });
}

Task<void> async_highlight_dispenser(const PodRegistry& pod_registry, const PodId pod_id, const DispenserId dispenser_id)
{
    log::Logger logger{log::create_or_get("station_state_bridge")};
    auto dispenser = pod_registry.dispenser_of_pod(pod_id, dispenser_id);
    if (not dispenser.has_value()) {
        SPDLOG_LOGGER_ERROR(logger, "Could not find a dispenser with id {} or pod with id {}", dispenser_id, pod_id);
        co_return;
    }
    try {
        co_await (*dispenser)->highlight(std::chrono::seconds{2});
    }
    catch (const std::exception& err) {
        SPDLOG_LOGGER_ERROR(logger, "Error while highlighting: {}", err.what());
    }
}

Task<void> async_mark_dispenser_refilled(const PodRegistry& pod_registry,
                                         FillLevelTracker& fill_levels,
                                         const PodId pod_id,
                                         const DispenserId dispenser_id)
{
    log::Logger logger{log::create_or_get("station_state_bridge")};
    auto dispenser = pod_registry.dispenser_of_pod(pod_id, dispenser_id);
    if (not dispenser.has_value()) {
        SPDLOG_LOGGER_ERROR(logger, "Could not find a dispenser with id {} or pod with id {}", dispenser_id, pod_id);
        co_return;
    }
    co_await fill_levels.refilled(PodDispenser{pod_id, dispenser_id}, **dispenser, RefillSource::admin_area);
}

Task<void> async_emergency_stop_all(const PodRegistry& pod_registry)
{
    try {
        co_await pod_registry.force_safe_state_all_pods();
    }
    catch (const std::exception& err) {
        SPDLOG_LOGGER_ERROR(log::create_or_get("station_state_bridge"), "Emergency stop failed: {}", err.what());
    }
}

void StationStateBridge::on_navigate_to(const Page target)
{
    if (target == Page::AdminAreaPage && !operator_authenticated_) {
        SPDLOG_LOGGER_WARN(logger_, "Requested maintenance access denied. Operator not authenticated.");
        return;
    }
    if (target != Page::AdminAreaPage) {
        operator_authenticated_ = false; // relock on leaving admin area
    }
    const Page previous = ui_->global<StationStateContext>().get_active_screen();
    ui_->global<StationStateContext>().set_active_screen(target);
    ui_->global<StationStateContext>().invoke_navigated_to(previous, target);
}

void StationStateBridge::on_submit_operator_pin(const slint::SharedString& pin)
{
    const auto now = std::chrono::steady_clock::now();
    operator_authenticated_ = operator_auth_.verify(std::string{pin}, now);
    publish_operator_state(now);

    if (operator_authenticated_) {
        ui_->invoke_close_operator_pin_popup();
        on_navigate_to(Page::AdminAreaPage);
    }
    else {
        SPDLOG_LOGGER_WARN(logger_, "Operator entered a wrong PIN. Remaining attempts: {}", operator_auth_.remaining_attempts());
    }

    if (operator_auth_.is_locked_out(now) && not lockout_timer_.running()) {
        SPDLOG_LOGGER_DEBUG(logger_, "Starting operator lockout countdown.");
        lockout_timer_.start(slint::TimerMode::Repeated, std::chrono::seconds{1}, [this]() {
            const auto now = std::chrono::steady_clock::now();
            publish_operator_state(now);
            if (not operator_auth_.is_locked_out(now)) {
                SPDLOG_LOGGER_DEBUG(logger_, "Finished operator lockout countdown.");
                lockout_timer_.stop();
            }
        });
    }
}

void StationStateBridge::publish_operator_state(const std::chrono::steady_clock::time_point now)
{
    const auto& ctx = ui_->global<StationStateContext>();
    ctx.set_operator_locked_out(operator_auth_.is_locked_out(now));
    ctx.set_operator_remaining_attempts(operator_auth_.remaining_attempts());
    ctx.set_operator_lockout_seconds_remaining(static_cast<int>(operator_auth_.lockout_remaining(now).count()));
}

PodStateImpl::~PodStateImpl()
{
    try {
        if (auto p = parent_.lock(); p != nullptr) {
            p->pod_about_to_removed(*this);
        }
    }
    catch (const std::exception& ex) {
        SPDLOG_LOGGER_ERROR(
            log::create_or_get("pod_state"), "Could not invoke station state remove pod action. Reason: {}", ex.what());
    }
}

void PodStateImpl::update_state(ConnectionState state)
{
    data_.connection_state = [state]() -> gui::ConnectionState {
        switch (state) {
        case PodState::ConnectionState::unknown:
        case PodState::ConnectionState::disconnected:
            return gui::ConnectionState::Disconnected;
        case PodState::ConnectionState::connecting:
            return gui::ConnectionState::Connecting;
        case PodState::ConnectionState::connected:
            return gui::ConnectionState::Connected;
        case PodState::ConnectionState::incompatible:
            return gui::ConnectionState::Incompatible;
        }
        std::unreachable();
    }();
    if (auto p = parent_.lock(); p != nullptr) {
        p->pod_connection_state_changed(*this);
    }
}

void PodStateImpl::update_info(PodInfo info)
{
    data_.info = std::move(info);
    if (auto p = parent_.lock(); p != nullptr) {
        p->pod_info_changed(*this);
    }
}
} // namespace cm::gui
