module;
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <stdexec/execution.hpp>

module cm.sim:simulated_pod_discovery_impl;
import std;
import cm.core;
import cm;
import :client;
import :simulated_pod_discovery;

namespace cm::sim {
namespace {
Task<void> run_client(std::shared_ptr<Client<Socket>> c)
{
    try {
        co_await c->run();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Client exited: {}", e.what());
    }
}
} // namespace

Task<void> SimulatedPodDiscovery::discover(AsyncScope& scope, std::function<void(std::shared_ptr<IPod>)> on_pod_discovered)
{
    // A bottle that runs dry fills itself again, because nobody can refill a simulated bottle by hand.
    const ClientConfig config{.auto_refill_after = std::chrono::seconds{10}};
    auto clients = std::array{
        std::make_shared<Client<Socket>>(scope, "Client1", Version{.major = 1}, config),
        std::make_shared<Client<Socket>>(scope, "Client2", Version{.major = 1}, config),
    };
    for (auto&& c : clients) {
        // A nearly empty bottle makes the warning and the refill dialog easy to reach by hand.
        c->bottle(DispenserId{0}).liquid = 60 * units::si::gram;
        c->power_cycle_load_cells();

        Socket server_socket{c->socket().get_executor()};

        boost::asio::local::connect_pair(c->socket(), server_socket);

        scope.spawn(run_client(c));
        on_pod_discovered(std::make_shared<Pod>(std::make_unique<SocketIoStream<Socket>>(std::move(server_socket))));
    }

    co_return;
}
} // namespace cm::sim
