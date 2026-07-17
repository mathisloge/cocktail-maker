#include <boost/asio/as_tuple.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <slint.h>
#include "app-window.h"

import std;
import cm;
import cm.gui;

namespace {

inline void flush_slint_events()
{
    slint::invoke_from_event_loop([]() { slint::quit_event_loop(); });
    slint::run_event_loop(slint::EventLoopMode::RunUntilQuit);
}

} // namespace

TEST_CASE("async_show_manual_command_popup - opens and resolves on confirm", "[gui][async_ui]")
{
    auto ui = cm::gui::AppWindow::create();
    cm::IngredientStore ingredient_store;

    // Caller (here: the test, mirroring MachineAdapter) does the transform.
    auto wrapped = cm::Command{cm::ManualCommand{.instruction = "Refill ingredient"}};
    auto ui_command = cm::gui::transform_command(wrapped, ingredient_store);
    REQUIRE(ui_command.has_value());

    boost::asio::io_context ctx;
    bool completed = false;
    boost::system::error_code captured_ec;

    auto coro = [&]() -> boost::cobalt::task<void> {
        auto [ec, result] =
            co_await cm::gui::async_show_manual_command_popup(ui, *ui_command, boost::asio::as_tuple(boost::cobalt::use_op));
        captured_ec = ec;
        completed = true;
    };

    std::future<void> fut = boost::cobalt::spawn(ctx, coro(), boost::asio::use_future);

    ctx.poll();           // drives to first co_await -> queues popup-open onto Slint's loop
    flush_slint_events(); // opens popup, wires on_manual_command_confirmed()

    ui->invoke_manual_command_confirmed(); // simulate user confirm; verify against app-window.slint
    flush_slint_events();

    ctx.run();

    // Block until the coroutine frame is fully cleaned up
    fut.get();

    REQUIRE(completed);
    REQUIRE_FALSE(captured_ec);
}

TEST_CASE("async_show_manual_command_popup - cancellation closes the popup", "[gui][async_ui]")
{
    auto ui = cm::gui::AppWindow::create();
    cm::IngredientStore ingredient_store;

    auto wrapped = cm::Command{cm::ManualCommand{.instruction = "Refill ingredient"}};
    auto ui_command = cm::gui::transform_command(wrapped, ingredient_store);
    REQUIRE(ui_command.has_value());

    boost::asio::io_context ctx;
    bool completed = false;
    boost::system::error_code captured_ec;
    boost::asio::cancellation_signal cancel_signal;

    auto coro = [&]() -> boost::cobalt::task<void> {
        auto [ec, result] = co_await cm::gui::async_show_manual_command_popup(
            ui,
            *ui_command,
            boost::asio::as_tuple(boost::asio::bind_cancellation_slot(cancel_signal.slot(), boost::cobalt::use_op)));
        captured_ec = ec;
        completed = true;
    };

    // Spawn with use_future to guarantee completion tracking
    std::future<void> fut = boost::cobalt::spawn(ctx, coro(), boost::asio::use_future);

    ctx.poll();
    flush_slint_events();

    cancel_signal.emit(boost::asio::cancellation_type::terminal);
    flush_slint_events(); // runs the invoke_close_manual_command_popup() closure

    ctx.run();

    // Block until the coroutine frame is fully cleaned up
    fut.get();

    REQUIRE(completed);
    REQUIRE(captured_ec == boost::asio::error::operation_aborted);
}
