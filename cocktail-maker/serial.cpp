module;
#include <boost/asio/serial_port.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <spdlog/spdlog.h>

#ifdef __linux__
#include <boost/asio/posix/stream_descriptor.hpp>
#include <libudev.h>
#else
#warning "USB hotplug discovery is only implemented for Linux. Returning immediately on other systems."
#endif

module cm:serial_impl;
import std;
import cm.core;
import :serial;

namespace cm {

#ifdef __linux__
namespace {

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

struct UdevEnumerateDeleter
{
    void operator()(udev_enumerate* e)
    {
        udev_enumerate_unref(e);
    }
};

using udev_ptr = std::unique_ptr<udev, UdevDeleter>;
using udev_monitor_ptr = std::unique_ptr<udev_monitor, UdevMonitorDeleter>;
using udev_device_ptr = std::unique_ptr<udev_device, UdevDeviceDeleter>;
using udev_enumerate_ptr = std::unique_ptr<udev_enumerate, UdevEnumerateDeleter>;

constexpr std::string_view TARGET_VID = "2fe3"; // TODO: Replace with Vendor ID
constexpr std::string_view TARGET_PID = "0001"; // TODO: Replace with Product ID

/// A pod exposes two CDC-ACM functions on a single USB device: one carries the
/// firmware's console/log text, the other the binary protocol frames. Both show
/// up as /dev/ttyACM*, and which of the two gets the lower number is a race
/// between the instances during enumeration - so they are told apart by the USB
/// interface string descriptor instead.
///
/// Firmware side that string is the `label` property of the cdc_acm_uart1 node
/// in the pod's boards/btt_octopus_v1.overlay; Zephyr publishes it as the
/// iInterface of the CDC control interface, which is exactly the interface the
/// kernel's cdc_acm driver binds the ttyACM to. Keep the two in sync.
constexpr std::string_view PROTOCOL_INTERFACE_NAME = "Cocktailmaker Pod Protocol";

/// Fallback for the case that the interface string descriptor cannot be read:
/// the protocol is the second of the pod's two CDC-ACM functions, so its control
/// interface - the one the ttyACM hangs off - is interface 2 (the log function
/// occupies 0 and 1). Less robust than the string, because it silently depends
/// on the order the firmware registers its USB classes in.
constexpr std::string_view PROTOCOL_INTERFACE_NUMBER = "02";

struct UsbTtyInfo
{
    std::string devnode;
    std::string vendor_id;
    std::string product_id;
    std::string device_serial;    // may be empty
    std::string interface_name;   // may be empty when the descriptor is unreadable
    std::string interface_number; // may be empty
};

std::string sysattr_or_empty(udev_device* dev, const char* name)
{
    const char* value = udev_device_get_sysattr_value(dev, name);
    return value != nullptr ? std::string{value} : std::string{};
}

/// Collects the identifying bits of a tty that belongs to a USB device.
/// Returns nullopt for ttys that are not USB-backed at all (serial-over-PCI,
/// pseudo terminals, ...), which is the bulk of what the monitor reports.
///
/// Note that both parent lookups return references owned by `dev` - they are
/// borrowed and must not be unref'd.
std::optional<UsbTtyInfo> describe_usb_tty(udev_device* dev)
{
    const char* devnode = udev_device_get_devnode(dev);
    if (devnode == nullptr) {
        return std::nullopt;
    }

    udev_device* usb_device = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    if (usb_device == nullptr) {
        return std::nullopt;
    }

    /// The interface node carries the per-function identity; the device node
    /// above it only says which pod this is, not which of its two ports.
    udev_device* usb_interface = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_interface");
    if (usb_interface == nullptr) {
        return std::nullopt;
    }

    return UsbTtyInfo{
        .devnode = devnode,
        /// Read straight from sysfs rather than via ID_VENDOR_ID/ID_MODEL_ID,
        /// which only exist once udev's usb_id builtin has run for the device.
        .vendor_id = sysattr_or_empty(usb_device, "idVendor"),
        .product_id = sysattr_or_empty(usb_device, "idProduct"),
        .device_serial = sysattr_or_empty(usb_device, "serial"),
        .interface_name = sysattr_or_empty(usb_interface, "interface"),
        .interface_number = sysattr_or_empty(usb_interface, "bInterfaceNumber"),
    };
}

bool is_pod_device(const UsbTtyInfo& info)
{
    return info.vendor_id == TARGET_VID && info.product_id == TARGET_PID;
}

bool is_protocol_port(const UsbTtyInfo& info)
{
    if (!info.interface_name.empty()) {
        return info.interface_name == PROTOCOL_INTERFACE_NAME;
    }

    return info.interface_number == PROTOCOL_INTERFACE_NUMBER;
}

} // namespace
#endif

boost::cobalt::generator<std::shared_ptr<IPod>> SerialPodDiscovery::discover()
{
    auto logger = cm::log::create_or_get("serial");

#ifndef __linux__
    // On non-Linux systems, the generator exits immediately.
    SPDLOG_LOGGER_ERROR(logger, "USB hotplug discovery is only implemented for Linux. Aborting discovery.");
    throw SerialInitializationException{};
#else

    auto exec = co_await boost::cobalt::this_coro::executor;

    udev_ptr udev_ctx{udev_new()};
    if (!udev_ctx) {
        SPDLOG_LOGGER_ERROR(logger, "Could not create udev context.");
        throw SerialInitializationException();
    }

    udev_monitor_ptr mon{udev_monitor_new_from_netlink(udev_ctx.get(), "udev")};
    if (!mon) {
        SPDLOG_LOGGER_ERROR(logger, "Could not create udev monitor.");
        throw SerialMonitorException();
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon.get(), "tty", nullptr);
    udev_monitor_enable_receiving(mon.get());

    const int fd = udev_monitor_get_fd(mon.get());
    boost::asio::posix::stream_descriptor monitor_descriptor(exec, fd);

    SPDLOG_LOGGER_INFO(logger,
                       "Watching udev for tty devices with VID '{}', PID '{}' and USB interface '{}'.",
                       TARGET_VID,
                       TARGET_PID,
                       PROTOCOL_INTERFACE_NAME);

    /// Devices already plugged in when we start would never produce an "add"
    /// event, so they have to be picked up by an explicit scan. The monitor is
    /// deliberately armed *before* that scan: a pod plugged in during the scan
    /// is then reported by both, and the duplicate is filtered below, whereas
    /// the reverse order would drop it entirely.
    std::set<std::string> opened_devnodes;

    auto open_pod = [&](const UsbTtyInfo& info) -> std::shared_ptr<IPod> {
        if (!opened_devnodes.insert(info.devnode).second) {
            SPDLOG_LOGGER_TRACE(logger, "Serial pod at '{}' is already open, skipping.", info.devnode);
            return nullptr;
        }

        SPDLOG_LOGGER_INFO(logger,
                           "Matching serial pod found at '{}' (interface '{}' #{}, device serial '{}').",
                           info.devnode,
                           info.interface_name,
                           info.interface_number,
                           info.device_serial);
        try {
            boost::asio::serial_port serial_port(exec, info.devnode);

            serial_port.set_option(boost::asio::serial_port_base::baud_rate(115200));
            serial_port.set_option(boost::asio::serial_port_base::character_size(8));

            SPDLOG_LOGGER_DEBUG(logger, "Opened serial port '{}'.", info.devnode);

            return std::make_shared<Pod>(
                std::make_unique<SocketIoStream<boost::asio::serial_port>>(std::move(serial_port)));
        }
        catch (const std::exception& e) {
            opened_devnodes.erase(info.devnode);
            SPDLOG_LOGGER_ERROR(logger, "Failed to open serial port '{}': {}", info.devnode, e.what());
            throw SerialPortOpenException(info.devnode, e.what());
        }
    };

    {
        udev_enumerate_ptr enumerate{udev_enumerate_new(udev_ctx.get())};
        if (!enumerate) {
            SPDLOG_LOGGER_ERROR(logger, "Could not create udev enumerator.");
            throw SerialInitializationException();
        }

        udev_enumerate_add_match_subsystem(enumerate.get(), "tty");
        udev_enumerate_scan_devices(enumerate.get());

        udev_list_entry* entry = nullptr;
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate.get()))
        {
            udev_device_ptr dev{udev_device_new_from_syspath(udev_ctx.get(), udev_list_entry_get_name(entry))};
            if (!dev) {
                continue;
            }

            const auto info = describe_usb_tty(dev.get());
            if (!info || !is_pod_device(*info) || !is_protocol_port(*info)) {
                continue;
            }

            if (auto pod = open_pod(*info)) {
                co_yield std::move(pod);
            }
        }
    }

    for (;;) {
        co_await monitor_descriptor.async_wait(boost::asio::posix::descriptor_base::wait_read, boost::cobalt::use_op);

        udev_device_ptr dev{udev_monitor_receive_device(mon.get())};
        if (!dev) {
            SPDLOG_LOGGER_TRACE(logger, "Received empty udev device event, skipping.");
            continue;
        }

        const char* action_cstr = udev_device_get_action(dev.get());
        const std::string action = action_cstr ? action_cstr : "";

        /// Unplugging is not reported through this generator: the pod's session
        /// ends on its own once reads on the vanished port start failing, and
        /// the registry entry is dropped with it (see run_pod()). All we have to
        /// do here is let the devnode be opened again on the next "add".
        if (action == "remove") {
            const char* devnode = udev_device_get_devnode(dev.get());
            if (devnode != nullptr && opened_devnodes.erase(devnode) > 0) {
                SPDLOG_LOGGER_INFO(logger, "Serial pod at '{}' was removed.", devnode);
            }
            continue;
        }

        if (action != "add") {
            continue;
        }

        const auto info = describe_usb_tty(dev.get());
        if (!info) {
            continue;
        }

        if (!is_pod_device(*info)) {
            SPDLOG_LOGGER_TRACE(logger,
                                "Ignoring tty device '{}' with non-matching VID '{}' / PID '{}'.",
                                info->devnode,
                                info->vendor_id,
                                info->product_id);
            continue;
        }

        if (!is_protocol_port(*info)) {
            /// Expected for every pod: this is its log/console port, which
            /// carries human-readable text rather than protocol frames.
            SPDLOG_LOGGER_DEBUG(logger,
                                "Ignoring pod tty device '{}': USB interface '{}' (#{}) is not the protocol port.",
                                info->devnode,
                                info->interface_name,
                                info->interface_number);
            continue;
        }

        if (auto pod = open_pod(*info)) {
            co_yield std::move(pod);
        }
    }
#endif
}

} // namespace cm
