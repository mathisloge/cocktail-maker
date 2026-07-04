module;
#include <boost/asio.hpp>
#include <boost/cobalt.hpp>

export module cm.sim:simulated_pod_discovery;
import std;
import cm.core;
import cm;
import :client;

namespace cobalt = boost::cobalt;

namespace cm::sim {
export class SimulatedPodDiscovery : public PodDiscovery
{
    Client<Socket>& client_;

  public:
    explicit SimulatedPodDiscovery(Client<Socket>& client)
        : client_{client}
    {
    }

    cobalt::generator<std::shared_ptr<IPod>> discover() override
    {
        co_await cobalt::this_coro::initial;

        Socket server_socket{client_.socket().get_executor()};

        boost::asio::local::connect_pair(client_.socket(), server_socket);
        boost::asio::post(client_.socket().get_executor(), [this]() { client_.run(); });
        co_yield std::make_shared<Pod<Socket>>(std::move(server_socket));
        co_return {};
    }
};
} // namespace cm::sim
