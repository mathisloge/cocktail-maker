#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <catch2/catch_test_macros.hpp>
import std;
import cm;
import cm.gui;

using namespace cm::gui;
namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

// --- Mock UI Implementierung ---
struct MockUI
{
    std::vector<std::function<void()>> task_queue;
    std::function<void()> on_close_cb;
    bool is_open = false;

    // Simuliert slint::invoke_from_event_loop
    void run_in_ui_thread(std::function<void()> task)
    {
        task_queue.push_back(std::move(task));
    }

    void open_manual_command_popup(cm::ManualCommand command)
    {
        is_open = true;
    }

    void close_manual_command_popup()
    {
        is_open = false;
        // Viele UIs feuern beim programmatischen Schließen trotzdem das Event
        if (on_close_cb) {
            on_close_cb();
        }
    }

    void set_on_manual_command_confirmed_callback(std::function<void()> cb)
    {
        on_close_cb = std::move(cb);
    }

    // Hilfsfunktion für den Test: Simuliert den Slint Event-Loop
    void flush_ui_events()
    {
        auto current_tasks = std::move(task_queue);
        task_queue.clear();
        for (auto& task : current_tasks) {
            task();
        }
    }
};

TEST_CASE("async_show_manual_command_popup success path", "[ui][async]")
{
    asio::io_context io;
    auto ui = std::make_shared<MockUI>();
    bool test_passed = false;

    cobalt::spawn(
        io.get_executor(),
        [&]() -> cobalt::task<void> {
            DialogResult result = co_await async_show_manual_command_popup(ui, cm::ManualCommand{}, cobalt::use_op);
            test_passed = true;
        }(),
        asio::detached);

    asio::post(io, [&]() {
        ui->flush_ui_events();
        CHECK(ui->is_open == true);
        ui->on_close_cb();
    });

    io.run();
    CHECK(test_passed == true);
}

TEST_CASE("async_show_manual_command_popup cancellation path", "[ui][async]")
{
    asio::io_context io;
    auto ui = std::make_shared<MockUI>();
    bool was_cancelled = false;

    cobalt::spawn(
        io.get_executor(),
        [&]() -> cobalt::task<void> {
            try {
                // use race to simulate cancellation
                asio::steady_timer timeout(io, std::chrono::milliseconds(50));

                auto result = co_await cobalt::race(async_show_manual_command_popup(ui, cm::ManualCommand{}, cobalt::use_op),
                                                    timeout.async_wait(cobalt::use_op));

                // Timer has won
                CHECK(result.index() == 1);
                was_cancelled = true;
            }
            catch (const boost::system::system_error& e) {
                if (e.code() == asio::error::operation_aborted) {
                    was_cancelled = true;
                }
            }
        }(),
        asio::detached);

    asio::steady_timer ui_simulator(io, std::chrono::milliseconds(10));
    ui_simulator.async_wait([&](auto) {
        ui->flush_ui_events();
        CHECK(ui->is_open == true);
    });

    asio::steady_timer verify_timer(io, std::chrono::milliseconds(100));
    verify_timer.async_wait([&](auto) {
        ui->flush_ui_events();
        CHECK(ui->is_open == false);
    });

    io.run();
    CHECK(was_cancelled == true);
}

TEST_CASE("Late UI Callback after Cancel is ignored safely", "[ui][async]")
{
    asio::io_context io;
    auto ui = std::make_shared<MockUI>();

    cobalt::spawn(
        io.get_executor(),
        [&]() -> cobalt::task<void> {
            try {
                // cancel directly
                asio::steady_timer timeout(io, std::chrono::milliseconds(1));
                co_await cobalt::race(async_show_manual_command_popup(ui, cm::ManualCommand{}, cobalt::use_op),
                                      timeout.async_wait(cobalt::use_op));
            }
            catch (...) {
            }
        }(),
        asio::detached);

    asio::post(io, [&]() { ui->flush_ui_events(); });

    asio::steady_timer late_fire(io, std::chrono::milliseconds(50));
    late_fire.async_wait([&](auto) {
        // fire callback after coroutine is removed
        REQUIRE_NOTHROW(ui->on_close_cb());
    });

    io.run();
    SUCCEED();
}
