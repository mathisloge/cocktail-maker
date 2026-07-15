module;
#include <boost/asio/async_result.hpp>
#include <boost/cobalt/op.hpp>
#include <slint.h>
#include "app-window.h"

export module cm.gui:async_ui;
import std;
import cm;

namespace cm::gui {
namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

export struct DialogResult
{
};

export template <typename CompletionToken = cobalt::use_op_t>
auto async_show_manual_command_popup(slint::ComponentHandle<AppWindow> ui,
                                     gui::Command ui_command,
                                     CompletionToken token = cobalt::use_op)
{
    return asio::async_initiate<CompletionToken, void(boost::system::error_code, DialogResult)>(
        [](auto handler, slint::ComponentHandle<AppWindow> ui, gui::Command ui_command) {
            using Handler = decltype(handler);

            struct SharedState
            {
                std::mutex mutex;
                std::optional<Handler> handler;
                std::optional<asio::executor_work_guard<asio::any_io_executor>> work_guard;
                slint::ComponentHandle<AppWindow> ui;

                SharedState(Handler h, asio::any_io_executor ex, slint::ComponentHandle<AppWindow> u)
                    : handler(std::move(h))
                    , work_guard(asio::make_work_guard(ex))
                    , ui(std::move(u))
                {
                }
            };

            auto exec = asio::get_associated_executor(handler);
            auto slot = asio::get_associated_cancellation_slot(handler);
            auto state = std::make_shared<SharedState>(std::move(handler), exec, ui);

            // register cancellation
            if (slot.is_connected()) {
                slot.assign([weak_state = std::weak_ptr<SharedState>(state)](asio::cancellation_type type) {
                    if (type == asio::cancellation_type::none) {
                        return;
                    }

                    if (auto st = weak_state.lock()) {
                        slint::invoke_from_event_loop([ui = st->ui]() { ui->invoke_close_manual_command_popup(); });

                        std::unique_lock<std::mutex> lock(st->mutex);
                        if (st->handler) {
                            auto h = std::move(*(st->handler));
                            st->handler.reset();

                            auto w = std::move(*(st->work_guard));
                            st->work_guard.reset();
                            lock.unlock();

                            asio::post(w.get_executor(),
                                       [h = std::move(h), w]() mutable { h(asio::error::operation_aborted, DialogResult{}); });
                        }
                    }
                });
            }

            slint::invoke_from_event_loop([state, ui_command = std::move(ui_command)]() mutable {
                state->ui->invoke_open_manual_command_popup(std::move(ui_command));

                state->ui->on_manual_command_confirmed([state]() {
                    state->ui->invoke_close_manual_command_popup();

                    std::unique_lock<std::mutex> lock(state->mutex);
                    if (state->handler) {
                        auto h = std::move(*(state->handler));
                        state->handler.reset();

                        auto w = std::move(*(state->work_guard));
                        state->work_guard.reset();
                        lock.unlock();

                        asio::post(w.get_executor(),
                                   [h = std::move(h), w]() mutable { h(boost::system::error_code{}, DialogResult{}); });
                    }
                });
            });
        },
        token,
        std::move(ui),
        std::move(ui_command));
}
} // namespace cm::gui
