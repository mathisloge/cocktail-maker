module;
#include <boost/asio/serial_port.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/this_coro.hpp>

#ifdef __linux__
#include <boost/asio/posix/stream_descriptor.hpp>
#include <libudev.h>
#else
#warning "USB hotplug discovery is only implemented for Linux. Returning immediately on other systems."
#endif

module cm:serial_impl;
import std;
import :serial;

namespace cm {

#ifdef __linux__
struct UdevDeleter
{
    void operator()(udev* u)
    {
        udev_unref(u);
    }
};

struct UdevMonitorDeleter
{
    void operator()(udev_monitor* m)
    {
        udev_monitor_unref(m);
    }
};

struct UdevDeviceDeleter
{
    void operator()(udev_device* d)
    {
        udev_device_unref(d);
    }
};

using udev_ptr = std::unique_ptr<udev, UdevDeleter>;
using udev_monitor_ptr = std::unique_ptr<udev_monitor, UdevMonitorDeleter>;
using udev_device_ptr = std::unique_ptr<udev_device, UdevDeviceDeleter>;
#endif

boost::cobalt::generator<std::shared_ptr<IPod>> SerialPodDiscovery::discover()
{
#ifndef __linux__
    // On non-Linux systems, the generator exits immediately.
    throw SerialInitializationException{};
#else

    auto exec = co_await boost::cobalt::this_coro::executor;

    constexpr std::string TARGET_VID = "1234"; // TODO: Replace with Vendor ID
    constexpr std::string TARGET_PID = "abcd"; // TODO: Replace with Product ID

    udev_ptr udev_ctx{udev_new()};
    if (!udev_ctx) {
        throw SerialInitializationException();
    }

    udev_monitor_ptr mon{udev_monitor_new_from_netlink(udev_ctx.get(), "udev")};
    if (!mon) {
        throw SerialMonitorException();
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon.get(), "tty", nullptr);
    udev_monitor_enable_receiving(mon.get());

    const int fd = udev_monitor_get_fd(mon.get());
    boost::asio::posix::stream_descriptor monitor_descriptor(exec, fd);

    for (;;) {
        co_await monitor_descriptor.async_wait(boost::asio::posix::descriptor_base::wait_read, boost::cobalt::use_op);

        udev_device_ptr dev{udev_monitor_receive_device(mon.get())};
        if (!dev) {
            continue;
        }

        const char* action_cstr = udev_device_get_action(dev.get());
        const std::string action = action_cstr ? action_cstr : "";

        if (action == "add") {
            const char* vendor_id = udev_device_get_property_value(dev.get(), "ID_VENDOR_ID");
            const char* model_id = udev_device_get_property_value(dev.get(), "ID_MODEL_ID");
            const char* devnode = udev_device_get_devnode(dev.get());

            if (vendor_id && model_id && devnode) {
                if (TARGET_VID == vendor_id && TARGET_PID == model_id) {
                    try {
                        boost::asio::serial_port serial_port(exec, devnode);

                        serial_port.set_option(boost::asio::serial_port_base::baud_rate(115200));
                        serial_port.set_option(boost::asio::serial_port_base::character_size(8));

                        co_yield std::make_shared<Pod>(
                            std::make_unique<SocketIoStream<boost::asio::serial_port>>(std::move(serial_port)));
                    }
                    catch (const std::exception& e) {
                        throw SerialPortOpenException(devnode, e.what());
                    }
                }
            }
        }
    }
#endif
}

} // namespace cm
