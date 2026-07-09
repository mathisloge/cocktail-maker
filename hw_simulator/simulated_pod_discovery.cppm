module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>
#include <spdlog/spdlog.h>

export module cm.sim:simulated_pod_discovery;
import std;
import cm.core;
import cm;
import :client;

namespace cobalt = boost::cobalt;

namespace cm::sim {
boost::cobalt::task<void> run_client(std::unique_ptr<Client<Socket>> c)
{
    try {
        co_await c->run();
    }
    catch (const std::exception& e) {
        SPDLOG_ERROR("Client exited: {}", e.what());
    }
}

export class SimulatedPodDiscovery : public PodDiscovery
{
  public:
    cobalt::generator<std::shared_ptr<IPod>> discover() override
    {
        co_await cobalt::this_coro::initial;
        auto ex = co_await cobalt::this_coro::executor;
        auto clients = std::array{
            std::make_unique<Client<Socket>>(ex, "Client1", Version{.major = 1}),
            std::make_unique<Client<Socket>>(ex, "Client2", Version{.major = 1}),
        };
        for (auto&& c : clients) {
            Socket server_socket{c->socket().get_executor()};

            boost::asio::local::connect_pair(c->socket(), server_socket);

            boost::cobalt::spawn(c->socket().get_executor(), run_client(std::move(c)), boost::asio::detached);
            co_yield std::make_shared<Pod>(std::make_unique<SocketIoStream<Socket>>(std::move(server_socket)));
        }

        co_return {};
    }
};
} // namespace cm::sim
