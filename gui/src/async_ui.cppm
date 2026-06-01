module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <slint.h>
#include "app-window.h"

export module cm.gui:async_ui;
import std;
import cm;
import :recipe_adapter;

namespace cm::gui {
namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

export struct DialogResult
{
};

export template <typename T>
concept AsyncUiLike = requires(T a, std::function<void()> task, std::function<void()> cb, cm::ManualCommand manual_command) {
    { a.run_in_ui_thread(task) } -> std::same_as<void>;
    { a.open_manual_command_popup(manual_command) } -> std::same_as<void>;
    { a.close_manual_command_popup() } -> std::same_as<void>;
    { a.set_on_manual_command_confirmed_callback(cb) } -> std::same_as<void>;
};

export template <AsyncUiLike UI, typename CompletionToken = cobalt::use_op_t>
auto async_show_manual_command_popup(std::shared_ptr<UI> ui, cm::ManualCommand command, CompletionToken token = cobalt::use_op)
{
    return asio::async_initiate<CompletionToken, void(boost::system::error_code, DialogResult)>(
        [](auto handler, std::shared_ptr<UI> ui, cm::ManualCommand command) {
            using Handler = decltype(handler);

            struct SharedState
            {
                std::mutex mutex;
                std::optional<Handler> handler;
                std::optional<asio::executor_work_guard<asio::any_io_executor>> work_guard;
                std::shared_ptr<UI> ui;

                SharedState(Handler h, asio::any_io_executor ex, std::shared_ptr<UI> u)
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
                        st->ui->run_in_ui_thread([ui = st->ui]() { ui->close_manual_command_popup(); });

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

            state->ui->run_in_ui_thread([state, command = std::move(command)]() {
                state->ui->open_manual_command_popup(command);

                state->ui->set_on_manual_command_confirmed_callback([state]() {
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
        std::move(command));
}

export struct SlintAsyncAdapter
{
    slint::ComponentHandle<AppWindow> ui;
    const IngredientStore& ingredient_store_;

    SlintAsyncAdapter(slint::ComponentHandle<AppWindow> ui_handle, const IngredientStore& ingredient_store)
        : ui{std::move(ui_handle)}
        , ingredient_store_{ingredient_store}
    {
    }

    void run_in_ui_thread(std::function<void()> task)
    {
        slint::invoke_from_event_loop([task = std::move(task)]() { task(); });
    }

    void open_manual_command_popup(cm::ManualCommand command)
    {
        auto wrapped_command = cm::Command{std::move(command)};
        auto ui_command = transform_command(wrapped_command, ingredient_store_);
        if (not ui_command.has_value()) {
            throw std::runtime_error("Command cannot be transformed into a ManualCommand");
        }
        ui->invoke_open_manual_command_popup(std::move(*ui_command));
    }

    void close_manual_command_popup()
    {
        ui->invoke_close_manual_command_popup();
    }

    void set_on_manual_command_confirmed_callback(std::function<void()> cb)
    {
        ui->on_manual_command_confirmed([this, cb = std::move(cb)]() {
            ui->invoke_close_manual_command_popup();
            cb();
        });
    }
};
} // namespace cm::gui
