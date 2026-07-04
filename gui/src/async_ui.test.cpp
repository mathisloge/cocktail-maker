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

struct AsyncTestFixture
{
    boost::asio::io_context ioc;
    std::shared_ptr<MockUI> ui = std::make_shared<MockUI>();

    AsyncTestFixture()
    {
        boost::cobalt::this_thread::set_executor(ioc.get_executor());
    }

    // Safely executes tests and guarantees graceful server shutdown even if assertions fail
    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        std::exception_ptr err;

        auto test_wrapper = [&, ui = ui]() -> boost::cobalt::task<void> {
            try {
                co_await test_coro(ui);
            }
            catch (...) {
                // Catch2 REQUIRE failures throw an exception to abort the execution path
                err = std::current_exception();
            }
        };

        boost::cobalt::spawn(ioc, test_wrapper(), boost::asio::detached);
        ioc.run();

        if (err) {
            std::rethrow_exception(err);
        }
    }
};

TEST_CASE_METHOD(AsyncTestFixture, "async_show_manual_command_popup success path", "[ui][async]")
{
    asio::steady_timer ui_simulator(ioc, std::chrono::milliseconds(10));
    ui_simulator.async_wait([ui = ui](auto) {
        ui->flush_ui_events();
        CHECK(ui->is_open == true);
        ui->on_close_cb();
    });
    run_test([&](auto ui) -> boost::cobalt::task<void> {
        DialogResult result = co_await async_show_manual_command_popup(ui, cm::ManualCommand{}, cobalt::use_op);
        CHECK(true);
    });
}

TEST_CASE_METHOD(AsyncTestFixture, "async_show_manual_command_popup cancellation path", "[ui][async]")
{
    asio::steady_timer ui_simulator(ioc, std::chrono::milliseconds(10));
    ui_simulator.async_wait([&](auto) {
        ui->flush_ui_events();
        CHECK(ui->is_open == true);
    });

    asio::steady_timer verify_timer(ioc, std::chrono::milliseconds(100));
    verify_timer.async_wait([&](auto) {
        ui->flush_ui_events();
        CHECK(ui->is_open == false);
    });

    run_test([](auto ui) -> boost::cobalt::task<void> {
        // use race to simulate cancellation
        asio::steady_timer timeout(co_await cobalt::this_coro::executor, std::chrono::milliseconds(50));
        auto result = co_await cobalt::race(async_show_manual_command_popup(ui, cm::ManualCommand{}, cobalt::use_op),
                                            timeout.async_wait(cobalt::use_op));
        // Timer has won
        CHECK(result.index() == 1);
    });
}

TEST_CASE_METHOD(AsyncTestFixture, "Late UI Callback after Cancel is ignored safely", "[ui][async]")
{
    asio::post(ioc, [&]() { ui->flush_ui_events(); });

    asio::steady_timer late_fire(ioc, std::chrono::milliseconds(50));
    late_fire.async_wait([ui = ui](auto) {
        // fire callback after coroutine is removed
        REQUIRE_THROWS(ui->on_close_cb());
    });

    run_test([](auto ui) -> boost::cobalt::task<void> {
        // cancel directly
        asio::steady_timer timeout(co_await cobalt::this_coro::executor, std::chrono::milliseconds(1));
        co_await cobalt::race(async_show_manual_command_popup(ui, cm::ManualCommand{}, cobalt::use_op),
                              timeout.async_wait(cobalt::use_op));
    });
}
