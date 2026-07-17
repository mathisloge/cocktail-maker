module;
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt.hpp> // this_thread::get_executor, cobalt::executor, cobalt::unique_handle
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

export class ManualCommandPopupAwaiter
{
  public:
    ManualCommandPopupAwaiter(slint::ComponentHandle<AppWindow> ui, gui::Command ui_command)
        : ui_command_(std::move(ui_command))
        , shared_(std::make_shared<Shared>(std::move(ui)))
    {
    }

    bool await_ready() const noexcept
    {
        return false;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> h)
    {
        // Executor the awaiting coroutine lives on (thread A). We must hop
        // back onto this before resuming it - never resume from slint thread.
        if constexpr (requires { h.promise().get_executor(); }) {
            shared_->executor = h.promise().get_executor();
        }
        else {
            shared_->executor = cobalt::this_thread::get_executor();
        }

        // Publish the handle before anything can possibly complete us.
        // Sequenced-before the slot.assign() below on this same thread,
        // so no synchronization is needed for this specific store.
        shared_->handle.store(h.address(), std::memory_order_release);

        // Wire cancellation straight from the awaiting promise.
        if constexpr (requires { h.promise().get_cancellation_slot(); }) {
            auto slot = h.promise().get_cancellation_slot();
            if (slot.is_connected()) {
                slot.assign([shared = std::weak_ptr(shared_)](asio::cancellation_type type) {
                    if (type == asio::cancellation_type::none) {
                        return;
                    }
                    if (auto st = shared.lock()) {
                        complete(st, asio::error::operation_aborted, DialogResult{});
                    }
                });
            }
        }

        // Open the popup on the Slint thread.
        slint::invoke_from_event_loop([shared = shared_, cmd = std::move(ui_command_)]() mutable {
            {
                std::lock_guard lock(shared->mutex);
                if (shared->completed) {
                    return; // already cancelled before we got here - don't open at all
                }
            }
            shared->ui->invoke_open_manual_command_popup(std::move(cmd));
            shared->ui->on_manual_command_confirmed(
                [shared]() mutable { complete(shared, boost::system::error_code{}, DialogResult{}); });
        });
    }

    std::tuple<boost::system::error_code, DialogResult> await_resume()
    {
        return {shared_->ec, shared_->result};
    }

  private:
    struct Shared
    {
        explicit Shared(slint::ComponentHandle<AppWindow> ui_)
            : ui(std::move(ui_))
        {
        }

        std::mutex mutex;
        bool completed = false;
        std::atomic<void*> handle{nullptr};
        cobalt::executor executor;
        slint::ComponentHandle<AppWindow> ui;
        boost::system::error_code ec;
        DialogResult result;
    };

    // Thread-safe "whoever gets here first wins" completion.
    // Called either from thread A (cancellation) or slint thread (confirm) -
    // the mutex guarantees exactly one of them ever proceeds past the guard.
    static void complete(const std::shared_ptr<Shared>& shared, boost::system::error_code ec, DialogResult result)
    {
        void* addr = nullptr;
        {
            std::lock_guard lock(shared->mutex);
            if (shared->completed) {
                return;
            }
            shared->completed = true;
            shared->ec = ec;
            shared->result = std::move(result);
            addr = shared->handle.exchange(nullptr, std::memory_order_acq_rel);
        }

        // Always tear down the popup and drop the on_manual_command_confirmed
        // closure's shared_ptr on the Slint thread, no matter which path
        // completed us - otherwise `shared` (and the ui handle) stays pinned
        // alive by that closure indefinitely.
        slint::invoke_from_event_loop([ui = shared->ui]() mutable {
            ui->invoke_close_manual_command_popup();
            ui->on_manual_command_confirmed([]() {});
        });

        // Hop back onto thread A and resume there. unique_handle owns the
        // coroutine handle until it's actually resumed (or destroys the
        // frame if it never gets posted, e.g. on an exception here).
        if (addr != nullptr) {
            boost::asio::post(shared->executor, cobalt::unique_handle<void>::from_address(addr));
        }
    }

    gui::Command ui_command_;
    std::shared_ptr<Shared> shared_;
};

export ManualCommandPopupAwaiter async_show_manual_command_popup(slint::ComponentHandle<AppWindow> ui, gui::Command ui_command)
{
    return ManualCommandPopupAwaiter{std::move(ui), std::move(ui_command)};
}

} // namespace cm::gui
