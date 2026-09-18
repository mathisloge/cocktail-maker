#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <slint.h>
#include <stdexec/execution.hpp>
#include "app-window.h"

import std;
import cm.core;
import cm;
import cm.gui;

namespace {

inline void flush_slint_events()
{
    slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
}

struct SlintFlusher
{
    ~SlintFlusher()
    {
        flush_slint_events();
    }
};
} // namespace

TEST_CASE("async_show_manual_command_popup - opens and resolves on confirm", "[gui][async_ui]")
{
    auto ui = cm::gui::AppWindow::create();
    SlintFlusher f;
    cm::IngredientStore ingredient_store;

    // Caller (here: the test, mirroring MachineAdapter) does the transform.
    auto wrapped = cm::Command{cm::ManualCommand{.instruction = "Refill ingredient"}};
    auto ui_command = cm::gui::transform_command(wrapped, ingredient_store);
    REQUIRE(ui_command.has_value());

    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    bool completed = false;

    auto coro = [&]() -> cm::Task<void> {
        co_await cm::gui::async_show_manual_command_popup(ui, *ui_command, ctx.get_executor());
        completed = true;
    };

    scope.spawn(coro());

    ctx.poll();           // drives to first co_await -> queues popup-open onto Slint's loop
    flush_slint_events(); // opens popup, wires on_manual_command_confirmed()

    ui->invoke_manual_command_confirmed(); // simulate user confirm; verify against app-window.slint
    flush_slint_events();

    // Block until the coroutine frame is fully cleaned up
    REQUIRE(cm::run_until_complete(ctx, scope.join()));

    REQUIRE(completed);
}

TEST_CASE("async_show_manual_command_popup - cancellation closes the popup", "[gui][async_ui]")
{
    auto ui = cm::gui::AppWindow::create();
    SlintFlusher f;
    cm::IngredientStore ingredient_store;

    auto wrapped = cm::Command{cm::ManualCommand{.instruction = "Refill ingredient"}};
    auto ui_command = cm::gui::transform_command(wrapped, ingredient_store);
    REQUIRE(ui_command.has_value());

    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    bool completed = false;
    bool stopped = false;

    auto coro = [&]() -> cm::Task<void> {
        co_await cm::gui::async_show_manual_command_popup(ui, *ui_command, ctx.get_executor());
        completed = true;
    };

    scope.spawn(coro() | stdexec::upon_stopped([&stopped]() noexcept { stopped = true; }));

    ctx.poll();
    flush_slint_events();

    scope.request_stop();
    flush_slint_events(); // runs the invoke_close_manual_command_popup() closure

    // Block until the coroutine frame is fully cleaned up
    REQUIRE(cm::run_until_complete(ctx, scope.join()));

    REQUIRE(stopped);
    REQUIRE_FALSE(completed);
}
