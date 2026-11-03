#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/when_any.hpp>
#include <stdexec/execution.hpp>

import std;
import cm.core;
import cm;
import cm.sim;

using namespace std::chrono_literals;

namespace {
using Socket = cm::sim::Socket;

class TestPodState final : public cm::PodState
{
  public:
    explicit TestPodState(cm::AwaitableBool& connected)
        : connected_{connected}
    {
    }

    void update_info(cm::PodInfo info) override
    {
        info_ = std::move(info);
    }

    cm::PodInfo info() const override
    {
        return info_;
    }

    void update_state(ConnectionState state) override
    {
        if (state == ConnectionState::connected) {
            connected_ = true;
        }
    }

  private:
    cm::AwaitableBool& connected_;
    cm::PodInfo info_;
};

/// A real `cm::Pod` that talks to a simulated client over a socket pair.
struct SimPodFixture
{
    boost::asio::io_context ctx;
    cm::AsyncScope scope{ctx};
    cm::AwaitableBool is_connected{ctx.get_executor()};
    std::shared_ptr<cm::sim::Client<Socket>> client =
        std::make_shared<cm::sim::Client<Socket>>(scope, "SimPod", cm::Version{.major = 2});
    std::shared_ptr<cm::Pod> pod = make_pod();

    std::shared_ptr<cm::Pod> make_pod()
    {
        Socket pod_socket{ctx};
        boost::asio::local::connect_pair(client->socket(), pod_socket);
        return std::make_shared<cm::Pod>(std::make_unique<cm::SocketIoStream<Socket>>(std::move(pod_socket)));
    }

    auto connected()
    {
        return is_connected.async_wait();
    }

    /// Runs `test_coro` while the pod and the client are running, and rethrows what it failed with.
    template <typename CoroutineFunc>
    void run_test(CoroutineFunc&& test_coro)
    {
        std::exception_ptr error;
        auto test_wrapper = [&]() -> cm::Task<void> {
            try {
                co_await exec::when_any(test_coro(), pod->run(std::make_unique<TestPodState>(is_connected)));
            }
            catch (...) {
                error = std::current_exception();
            }
        };

        scope.spawn(client->run());
        cm::run_until_complete(ctx, test_wrapper());

        scope.request_stop();
        cm::run_until_complete(ctx, scope.join());

        if (error) {
            std::rethrow_exception(error);
        }
    }
};
} // namespace

TEST_CASE_METHOD(SimPodFixture, "A calibration run returns the measured amount", "[sim][calibration]")
{
    run_test([this]() -> cm::Task<void> {
        co_await connected();
        const cm::units::Litre amount = co_await pod->dispenser_calibrate(cm::DispenserId{0}, 2s);
        CHECK(amount > 0 * cm::units::milli_litre);
    });
}
