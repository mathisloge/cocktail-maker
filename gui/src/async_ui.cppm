module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <slint.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

export module cm.gui:async_ui;
import std;
import cm;

namespace cm::gui {
namespace asio = boost::asio;

namespace detail {
/// Implemented by the waiting operation. It is only called on the io thread and at most once.
struct PopupCompletion
{
    void (*complete_confirmed)(PopupCompletion*) noexcept;
};

/// The state is shared between the waiting operation on the io thread and the popup's callbacks on the Slint thread.
struct PopupState
{
    PopupState(slint::ComponentHandle<AppWindow> ui_, asio::any_io_executor executor_)
        : ui(std::move(ui_))
        , executor(std::move(executor_))
    {
    }

    std::mutex mutex;
    bool completed = false;
    PopupCompletion* operation = nullptr;
    slint::ComponentHandle<AppWindow> ui;
    asio::any_io_executor executor;
};

// The io thread (stop request) and the Slint thread (confirm) race for the completion. The mutex guarantees that exactly
// one of them wins.
inline bool claim_completion(PopupState& state) noexcept
{
    std::lock_guard lock(state.mutex);
    if (state.completed) {
        return false;
    }
    state.completed = true;
    return true;
}

// The popup is torn down on the Slint thread, whichever path completed the operation. Otherwise the
// on_manual_command_confirmed closure keeps the state and the ui handle alive indefinitely.
inline void close_popup(const std::shared_ptr<PopupState>& state)
{
    slint::invoke_from_event_loop([ui = state->ui]() mutable {
        ui->invoke_close_manual_command_popup();
        ui->on_manual_command_confirmed([]() {});
    });
}

// Runs on the Slint thread when the operator confirms the popup.
inline void confirm(const std::shared_ptr<PopupState>& state)
{
    if (!claim_completion(*state)) {
        return;
    }
    close_popup(state);
    // The operation must only be completed on the io thread.
    asio::post(state->executor, [operation = state->operation]() noexcept { operation->complete_confirmed(operation); });
}
} // namespace detail

template <typename Receiver>
class ManualCommandPopupOperation : detail::PopupCompletion
{
  public:
    using operation_state_concept = stdexec::operation_state_tag;

    ManualCommandPopupOperation(slint::ComponentHandle<AppWindow> ui,
                                gui::Command ui_command,
                                asio::any_io_executor executor,
                                Receiver receiver)
        : detail::PopupCompletion{.complete_confirmed = &ManualCommandPopupOperation::finish_confirmed}
        , ui_command_{std::move(ui_command)}
        , state_{std::make_shared<detail::PopupState>(std::move(ui), std::move(executor))}
        , receiver_{std::move(receiver)}
    {
    }

    ManualCommandPopupOperation(ManualCommandPopupOperation&&) = delete;

    void start() & noexcept
    {
        const auto token = stdexec::get_stop_token(stdexec::get_env(receiver_));
        if (token.stop_requested()) {
            stdexec::set_stopped(std::move(receiver_));
            return;
        }
        state_->operation = this;
        on_stop_.emplace(token, OnStop{this});

        // Open the popup on the Slint thread.
        slint::invoke_from_event_loop([state = state_, cmd = std::move(ui_command_)]() mutable {
            {
                std::lock_guard lock(state->mutex);
                if (state->completed) {
                    return; // The operation was stopped before the popup could be opened.
                }
            }
            state->ui->invoke_open_manual_command_popup(std::move(cmd));
            state->ui->on_manual_command_confirmed([state]() { detail::confirm(state); });
        });
    }

  private:
    struct OnStop
    {
        ManualCommandPopupOperation* self;

        void operator()() const noexcept
        {
            self->finish_stopped();
        }
    };

    using StopToken = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
    using StopCallback = stdexec::stop_callback_for_t<StopToken, OnStop>;

    static void finish_confirmed(detail::PopupCompletion* completion) noexcept
    {
        auto* self = static_cast<ManualCommandPopupOperation*>(completion);
        self->on_stop_.reset();
        stdexec::set_value(std::move(self->receiver_));
    }

    void finish_stopped() noexcept
    {
        if (!detail::claim_completion(*state_)) {
            return; // The operator confirmed first, so the posted value completion wins.
        }
        detail::close_popup(state_);
        on_stop_.reset();
        stdexec::set_stopped(std::move(receiver_));
    }

    gui::Command ui_command_;
    std::shared_ptr<detail::PopupState> state_;
    Receiver receiver_;
    std::optional<StopCallback> on_stop_;
};

/// This sender shows the manual command popup and completes once the operator confirmed it. A stop request completes it
/// with `set_stopped`.
export class ManualCommandPopupSender
{
  public:
    using sender_concept = stdexec::sender_tag;

    ManualCommandPopupSender(slint::ComponentHandle<AppWindow> ui, gui::Command ui_command, asio::any_io_executor executor)
        : ui_{std::move(ui)}
        , ui_command_{std::move(ui_command)}
        , executor_{std::move(executor)}
    {
    }

    template <typename Self, typename... Env>
    static consteval auto get_completion_signatures() noexcept
    {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_stopped_t()>{};
    }

    template <stdexec::receiver Receiver>
    [[nodiscard]] ManualCommandPopupOperation<Receiver> connect(Receiver receiver) &&
    {
        return ManualCommandPopupOperation<Receiver>{
            std::move(ui_), std::move(ui_command_), std::move(executor_), std::move(receiver)};
    }

  private:
    slint::ComponentHandle<AppWindow> ui_;
    gui::Command ui_command_;
    asio::any_io_executor executor_;
};

/**
 * Shows the manual command popup for `ui_command` and waits for the operator to confirm it.
 *
 * It must be awaited on the thread that runs `executor`, which is also the thread it completes on. A stop request closes
 * the popup and completes with `set_stopped`.
 */
export ManualCommandPopupSender async_show_manual_command_popup(slint::ComponentHandle<AppWindow> ui,
                                                                gui::Command ui_command,
                                                                asio::any_io_executor executor)
{
    return ManualCommandPopupSender{std::move(ui), std::move(ui_command), std::move(executor)};
}

} // namespace cm::gui
