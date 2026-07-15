module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/cobalt/spawn.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include "app-window.h"

module cm.gui:station_state_bridge_impl;

import std;
import cm.core;
import cm;
import :station_state_bridge;
import :operator_auth;

namespace cm::gui {

boost::cobalt::task<void> async_highlight_dispenser(const PodRegistry& pod_registry,
                                                    const PodId pod_id,
                                                    const DispenserId dispenser_id);

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

    PodInfo info() const override
    {
        return data_.info;
    }

    void update_state(ConnectionState state) override;

  private:
    std::weak_ptr<StationStateBridge> parent_;
    PodStateData data_;
};

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
        const auto apply_state_to_ui = [](gui::Pod& gui_pod, const PodStateData& state_pod) {
            bool changed = false;
            if (gui_pod.id.data() != state_pod.info.id.raw()) {
                gui_pod.id = slint::SharedString{state_pod.info.id.raw()};
                changed = true;
            }
            if (gui_pod.connection_state != state_pod.connection_state) {
                gui_pod.connection_state = state_pod.connection_state;
                changed = true;
            }

            if ((gui_pod.dispensers == nullptr) or
                gui_pod.dispensers->row_count() != (state_pod.info.num_pumps + state_pod.info.num_valves)) {
                auto dispenser_model = std::make_shared<slint::VectorModel<gui::Dispenser>>();
                for (int i = 0; i < state_pod.info.num_pumps; ++i) {
                    dispenser_model->push_back(gui::Dispenser{
                        .id = i,
                        .type = DispenserType::Pump,
                    });
                }
                for (int i = 0; i < state_pod.info.num_valves; ++i) {
                    dispenser_model->push_back(gui::Dispenser{
                        .id = (state_pod.info.num_pumps + i),
                        .type = DispenserType::Valve,
                    });
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
            this->notify_row_added(row, 1);
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
    }

  private:
    std::vector<gui::Pod> pods_;
};

constexpr std::string kSalt = "static";

StationStateBridge::StationStateBridge(slint::ComponentHandle<AppWindow> ui,
                                       const PodRegistry& pod_registry,
                                       boost::asio::any_io_executor executor)
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
    ui_->global<StationStateContext>().on_highlight_dispenser(
        [&pod_registry, executor](const gui::Pod pod, const gui::Dispenser dispenser) {
            boost::cobalt::spawn(
                executor,
                async_highlight_dispenser(pod_registry, PodId{std::string{pod.id.data()}}, DispenserId{dispenser.id}),
                boost::asio::detached);
        });
    ui_->global<StationStateContext>().on_navigate_to([this](Page target) { on_navigate_to(target); });
    ui_->global<StationStateContext>().on_submit_operator_pin(
        [this, executor](slint::SharedString pin) { on_submit_operator_pin(pin, executor); });
}

std::unique_ptr<PodState> StationStateBridge::create_pod_state()
{
    auto pod_state = std::make_unique<PodStateImpl>(shared_from_this());
    pods_.emplace_back(pod_state.get());
    return pod_state;
}

void StationStateBridge::pod_connection_state_changed(const PodStateImpl& pod)
{
    update_pod_model(pod);
    const bool all_connected = (not pods_.empty()) and std::ranges::all_of(pods_, [](const auto& p) {
                                   return p->data().connection_state == gui::ConnectionState::Connected;
                               });

    // update device ready to all_connected
    slint::invoke_from_event_loop(
        [connected = all_connected, ui = ui_]() { ui->global<cm::gui::StationStateContext>().set_station_ready(connected); });
}

void StationStateBridge::pod_about_to_removed(const PodStateImpl& pod)
{
    std::erase(pods_, &pod);
    pod_connection_state_changed(pod);
    // ^^ always remove pod after connection state re-eval, since otherwise it will be added to the model again.
    slint::invoke_from_event_loop([model = pod_model_, pod = pod.data()]() { model->remove_pod(pod); });
}

void StationStateBridge::pod_info_changed(const PodStateImpl& pod)
{
    if (auto p = find_pod(&pod); p.has_value()) {
        update_pod_model(*p.value());
    }
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

boost::cobalt::task<void> async_highlight_dispenser(const PodRegistry& pod_registry,
                                                    const PodId pod_id,
                                                    const DispenserId dispenser_id)
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

void StationStateBridge::on_submit_operator_pin(const slint::SharedString& pin, boost::asio::any_io_executor executor)
{
    const auto now = std::chrono::steady_clock::now();
    operator_authenticated_ = operator_auth_.verify(std::string{pin}, now);
    const bool locked_out = operator_auth_.is_locked_out(now);

    const auto& ctx = ui_->global<StationStateContext>();
    ctx.set_operator_locked_out(operator_auth_.is_locked_out(now));
    ctx.set_operator_remaining_attempts(operator_auth_.remaining_attempts());
    ctx.set_operator_lockout_seconds_remaining(static_cast<int>(operator_auth_.lockout_remaining(now).count()));
    if (operator_authenticated_) {
        ui_->invoke_close_operator_pin_popup();
        on_navigate_to(Page::AdminAreaPage);
    }
    else {
        SPDLOG_LOGGER_WARN(
            logger_, "Entered wrong operator pin {}. Remaining: {}", pin.data(), operator_auth_.remaining_attempts());
    }
    if (locked_out) {
        boost::cobalt::spawn(executor, async_handle_operator_lockout(), boost::asio::detached);
    }
}

boost::cobalt::task<void> StationStateBridge::async_handle_operator_lockout()
{
    SPDLOG_LOGGER_DEBUG(logger_, "Starting operator lockout timeout.");
    for (auto now = std::chrono::steady_clock::now(); operator_auth_.is_locked_out(now); now = std::chrono::steady_clock::now()) {
        boost::asio::steady_timer timer{co_await boost::cobalt::this_coro::executor};
        timer.expires_after(std::chrono::seconds{1});
        co_await timer.async_wait(boost::cobalt::use_op);
        slint::invoke_from_event_loop([this]() {
            const auto now = std::chrono::steady_clock::now();
            const auto& ctx = ui_->global<StationStateContext>();
            ctx.set_operator_lockout_seconds_remaining(static_cast<int>(operator_auth_.lockout_remaining(now).count()));
        });
    }
    slint::invoke_from_event_loop([this]() {
        const auto now = std::chrono::steady_clock::now();
        const auto& ctx = ui_->global<StationStateContext>();
        ctx.set_operator_locked_out(operator_auth_.is_locked_out(now));
        ctx.set_operator_remaining_attempts(operator_auth_.remaining_attempts());
        ctx.set_operator_lockout_seconds_remaining(static_cast<int>(operator_auth_.lockout_remaining(now).count()));
    });
    SPDLOG_LOGGER_DEBUG(logger_, "Finished operator lockout timeout.");
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
