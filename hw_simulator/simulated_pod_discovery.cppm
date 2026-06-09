module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>

export module cm.sim:simulated_pod_discovery;
import std;
import cm;
import :client;

namespace cobalt = boost::cobalt;

namespace cm::sim {
export class SimulatedPodDiscovery : public PodDiscovery
{
    Client<Socket> client_;

  public:
    SimulatedPodDiscovery(cobalt::executor ex)
        : client_{Socket{ex}, "Client1", {.major = 1}}
    {
    }

    cobalt::generator<std::unique_ptr<IPod>> discover() override
    {
        co_await cobalt::this_coro::initial;

        Socket server_socket{client_.socket().get_executor()};

        boost::asio::local::connect_pair(client_.socket(), server_socket);
        boost::asio::post(client_.socket().get_executor(), [this]() { client_.run(); });
        co_yield std::make_unique<Pod<Socket>>(std::move(server_socket));
        co_return {};
    }
};
} // namespace cm::sim
