module;
#include <boost/cobalt/generator.hpp>

export module cm:serial;
import std;
import cm.core;
import :pod_discovery;

namespace cm {

export class SerialPodDiscovery : public PodDiscovery
{
  public:
    boost::cobalt::generator<std::shared_ptr<IPod>> discover() override;
};

export class SerialInitializationException : public std::runtime_error
{
  public:
    SerialInitializationException()
        : std::runtime_error("Could not create serial context.")
    {
    }
};

export class SerialMonitorException : public std::runtime_error
{
  public:
    SerialMonitorException()
        : std::runtime_error("Could not create serial monitor.")
    {
    }
};

export class SerialPortOpenException : public std::runtime_error
{
  public:
    explicit SerialPortOpenException(const std::string& devnode, const std::string& details)
        : std::runtime_error(std::format("Failed to open serial port [{}]: {}", devnode, details))
    {
    }
};

} // namespace cm