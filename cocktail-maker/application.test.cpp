#include <boost/asio/steady_timer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <exec/asio/use_sender.hpp>
#include <stdexec/execution.hpp>
import std;
import cm.core;
import cm;

using namespace std::chrono_literals;

namespace {
class TestApplication : public cm::Application
{
  public:
    using Application::run;
};

class NoPodsStationState : public cm::StationState
{
  public:
    std::unique_ptr<cm::PodState> create_pod_state() override
    {
        return nullptr;
    }
};

/// Never discovers anything and waits until it is stopped.
class WaitingPodDiscovery : public cm::PodDiscovery
{
  public:
    explicit WaitingPodDiscovery(std::atomic<bool>& stopped)
        : stopped_{stopped}
    {
    }

    cm::Task<void> discover(cm::AsyncScope& scope, std::function<void(std::shared_ptr<cm::IPod>)> /*on_pod_discovered*/) override
    {
        boost::asio::steady_timer timer{scope.executor(), 1h};
        co_await (timer.async_wait(exec::asio::use_sender) | stdexec::upon_stopped([this]() noexcept { stopped_ = true; }));
    }

  private:
    std::atomic<bool>& stopped_;
};
} // namespace

TEST_CASE("Application - shutdown stops and joins all asynchronous work", "[application]")
{
    std::atomic<bool> discovery_stopped{false};
    TestApplication app;
    app.run(std::make_shared<NoPodsStationState>(), std::make_unique<WaitingPodDiscovery>(discovery_stopped));

    app.shutdown();
    REQUIRE(discovery_stopped);

    // A second shutdown is a no-op.
    app.shutdown();
}

TEST_CASE("Application - shutdown without run is a no-op", "[application]")
{
    TestApplication app;
    app.shutdown();
}
