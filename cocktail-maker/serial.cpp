module;
#include <boost/asio/serial_port.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <boost/system/error_code.hpp>
#include <spdlog/spdlog.h>

#ifdef __linux__
#include <boost/asio/posix/descriptor_base.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <cerrno>
#include <fcntl.h>
#include <libudev.h>
#include <unistd.h>
#else
#warning "USB hotplug discovery is only implemented for Linux; discovery fails immediately on other systems."
#endif

module cm:serial_impl;
import std;
import cm.core;
import :serial;

namespace cm {

#ifdef __linux__
namespace {

namespace asio = boost::asio;
namespace cobalt = boost::cobalt;

// ── libudev handles ───────────────────────────────────────────────────────────

/// Turns one of libudev's `*_unref` functions into a unique_ptr deleter.
template <auto Unref>
struct UdevDeleter
{
    template <typename Handle>
    void operator()(Handle* handle) const noexcept
    {
        Unref(handle);
    }
};

using UdevPtr = std::unique_ptr<udev, UdevDeleter<udev_unref>>;
using UdevMonitorPtr = std::unique_ptr<udev_monitor, UdevDeleter<udev_monitor_unref>>;
using UdevDevicePtr = std::unique_ptr<udev_device, UdevDeleter<udev_device_unref>>;
using UdevEnumeratePtr = std::unique_ptr<udev_enumerate, UdevDeleter<udev_enumerate_unref>>;

/// libudev reports failures as negated errno values rather than through errno.
std::string errno_message(int error_number)
{
    return std::system_category().message(error_number);
}

std::string sysattr_or_empty(udev_device* dev, const char* name)
{
    const char* value = udev_device_get_sysattr_value(dev, name);
    return value != nullptr ? std::string{value} : std::string{};
}

// ── Recognising a pod's protocol port ─────────────────────────────────────────

constexpr std::string_view kTargetVid = "2fe3"; // TODO: Replace with Vendor ID
constexpr std::string_view kTargetPid = "0001"; // TODO: Replace with Product ID

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
constexpr std::string_view kProtocolInterfaceName = "Cocktailmaker Pod Protocol";

/// Fallback for the case that the interface string descriptor cannot be read:
/// the protocol is the second of the pod's two CDC-ACM functions, so its control
/// interface - the one the ttyACM hangs off - is interface 2 (the log function
/// occupies 0 and 1). Less robust than the string, because it silently depends
/// on the order the firmware registers its USB classes in.
constexpr std::string_view kProtocolInterfaceNumber = "02";

struct UsbTtyInfo
{
    std::string devnode;
    std::string vendor_id;
    std::string product_id;
    std::string device_serial;    // may be empty
    std::string interface_name;   // may be empty when the descriptor is unreadable
    std::string interface_number; // may be empty
};

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

    // The interface node carries the per-function identity; the device node
    // above it only says which pod this is, not which of its two ports.
    udev_device* usb_interface = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_interface");
    if (usb_interface == nullptr) {
        return std::nullopt;
    }

    return UsbTtyInfo{
        .devnode = devnode,
        // Read straight from sysfs rather than via ID_VENDOR_ID/ID_MODEL_ID,
        // which only exist once udev's usb_id builtin has run for the device.
        .vendor_id = sysattr_or_empty(usb_device, "idVendor"),
        .product_id = sysattr_or_empty(usb_device, "idProduct"),
        .device_serial = sysattr_or_empty(usb_device, "serial"),
        .interface_name = sysattr_or_empty(usb_interface, "interface"),
        .interface_number = sysattr_or_empty(usb_interface, "bInterfaceNumber"),
    };
}

bool is_pod_device(const UsbTtyInfo& info)
{
    return info.vendor_id == kTargetVid && info.product_id == kTargetPid;
}

bool is_protocol_port(const UsbTtyInfo& info)
{
    if (!info.interface_name.empty()) {
        return info.interface_name == kProtocolInterfaceName;
    }

    return info.interface_number == kProtocolInterfaceNumber;
}

/// What makes two ttys the same pod. The USB device serial is the pod's MCU id,
/// so it survives re-enumeration and, unlike the devnode, does not change when
/// the pod comes back as a different ttyACM number. Firmware without a serial
/// falls back to the devnode, which is the best that can be done there.
std::string pod_identity(const UsbTtyInfo& info)
{
    return info.device_serial.empty() ? info.devnode : info.device_serial;
}

/// The one place that decides whether a tty is a pod's protocol port. Both the
/// initial scan and the hotplug monitor go through it so the two cannot answer
/// the question differently.
std::optional<UsbTtyInfo> match_protocol_port(udev_device* dev, const log::Logger& logger)
{
    auto info = describe_usb_tty(dev);
    if (!info) {
        return std::nullopt;
    }

    if (!is_pod_device(*info)) {
        SPDLOG_LOGGER_TRACE(logger,
                            "Ignoring tty device '{}' with non-matching VID '{}' / PID '{}'.",
                            info->devnode,
                            info->vendor_id,
                            info->product_id);
        return std::nullopt;
    }

    if (!is_protocol_port(*info)) {
        // Expected for every pod: this is its log/console port, which carries
        // human-readable text rather than protocol frames.
        SPDLOG_LOGGER_DEBUG(logger,
                            "Ignoring pod tty device '{}': USB interface '{}' (#{}) is not the protocol port.",
                            info->devnode,
                            info->interface_name,
                            info->interface_number);
        return std::nullopt;
    }

    return info;
}

// ── Opening a pod ─────────────────────────────────────────────────────────────
constexpr unsigned int kPodBaudRate = 115200;
using OpenedPort = std::pair<boost::system::error_code, asio::serial_port>;

/// Opens `devnode` and puts it into the 8N1 line discipline the pod expects.
///
/// This might block.
OpenedPort open_and_configure(cobalt::executor exec, const std::string& devnode)
{
    boost::system::error_code ec;
    asio::serial_port port{std::move(exec)};
    port.open(devnode, ec);

    using serial_option = asio::serial_port_base;
    const auto apply = [&](const auto& option) {
        if (!ec) {
            port.set_option(option, ec);
        }
    };

    apply(serial_option::baud_rate(kPodBaudRate));
    apply(serial_option::character_size(8));
    // Spelled out instead of inherited from whatever state the port was left in
    // by its previous user - hardware flow control in particular would stall
    // every write until a modem line the pod does not drive gets asserted.
    apply(serial_option::parity(serial_option::parity::none));
    apply(serial_option::stop_bits(serial_option::stop_bits::one));
    apply(serial_option::flow_control(serial_option::flow_control::none));

    return {ec, std::move(port)};
}

/// Pod identity -> the devnode its session is currently running on.
using OpenPods = std::map<std::string, std::string>;

/// Opens one pod, or returns nullptr if it should not or cannot be opened.
std::shared_ptr<IPod> open_pod(cobalt::executor exec, OpenPods& open_pods, const log::Logger& logger, const UsbTtyInfo& info)
{
    const auto identity = pod_identity(info);

    // One physical pod gets one session. Keying this on the pod rather than on
    // the devnode matters because the same pod can be reachable through several
    // devnodes at once - a USB/IP export attached to two vhci ports, or an
    // attachment that has not been torn down yet - and a second session would
    // put two competing command streams on one machine.
    const auto [entry, is_new] = open_pods.try_emplace(identity, info.devnode);
    if (!is_new) {
        SPDLOG_LOGGER_DEBUG(
            logger, "Pod '{}' is already open on '{}', ignoring its second port '{}'.", identity, entry->second, info.devnode);
        return nullptr;
    }

    SPDLOG_LOGGER_INFO(logger,
                       "Matching serial pod found at '{}' (interface '{}' #{}, device serial '{}').",
                       info.devnode,
                       info.interface_name,
                       info.interface_number,
                       info.device_serial);

    // A port that refuses to open is reported and skipped rather than failing
    // discovery: discovery is the only thing that ever hands pods to the station,
    // so ending it over one unusable port would take every other pod - connected
    // and future - down with it. Dropping the pod from the table again lets a
    // later "add" event give it another try.
    auto [ec, port] = open_and_configure(exec, info.devnode);
    if (ec) {
        open_pods.erase(identity);
        SPDLOG_LOGGER_ERROR(
            logger, "Failed to open serial port '{}' ({}); skipping it: {}", info.devnode, ec.value(), ec.message());
        return nullptr;
    }

    SPDLOG_LOGGER_DEBUG(logger, "Opened serial port '{}'.", info.devnode);

    return std::make_shared<Pod>(std::make_unique<SocketIoStream<asio::serial_port>>(std::move(port)));
}

/// Drops the bookkeeping entry of a tty that has just disappeared, so the pod
/// behind it can be opened again when it comes back.
///
/// Unplugging is not reported through the generator: the pod's session ends on
/// its own once reads on the vanished port start failing, and the registry entry
/// is dropped with it (see run_pod()).
///
/// The lookup goes the other way round than the table is keyed, because a remove
/// event only carries the devnode - the sysfs attributes the pod's identity is
/// built from are already gone by then.
void forget_removed_port(OpenPods& open_pods, udev_device* dev, const log::Logger& logger)
{
    const char* devnode = udev_device_get_devnode(dev);
    if (devnode == nullptr) {
        return;
    }

    const auto removed = std::ranges::find_if(open_pods, [devnode](const auto& e) { return e.second == devnode; });
    if (removed == open_pods.end()) {
        return;
    }

    SPDLOG_LOGGER_INFO(logger, "Serial pod '{}' at '{}' was removed.", removed->first, devnode);
    open_pods.erase(removed);
}

// ── Watching udev ─────────────────────────────────────────────────────────────

UdevPtr make_udev_context(const log::Logger& logger)
{
    UdevPtr context{udev_new()};
    if (!context) {
        SPDLOG_LOGGER_ERROR(logger, "Could not create udev context.");
        throw SerialInitializationException{};
    }

    return context;
}

UdevMonitorPtr make_tty_monitor(udev& context, const log::Logger& logger)
{
    UdevMonitorPtr monitor{udev_monitor_new_from_netlink(&context, "udev")};
    if (!monitor) {
        SPDLOG_LOGGER_ERROR(logger, "Could not create udev monitor.");
        throw SerialMonitorException{};
    }

    // The subsystem filter is installed as a socket filter by the kernel, so
    // uevents for anything but ttys never even reach this process. It only takes
    // effect when receiving is enabled, which is why it goes first.
    if (const int rc = udev_monitor_filter_add_match_subsystem_devtype(monitor.get(), "tty", nullptr); rc < 0) {
        SPDLOG_LOGGER_ERROR(logger, "Could not install the udev 'tty' subsystem filter: {}", errno_message(-rc));
        throw SerialMonitorException{};
    }

    // Without this the monitor socket is never bound and next_event() would
    // block forever instead of failing, so the result really has to be checked.
    if (const int rc = udev_monitor_enable_receiving(monitor.get()); rc < 0) {
        SPDLOG_LOGGER_ERROR(logger, "Could not start receiving udev events: {}", errno_message(-rc));
        throw SerialMonitorException{};
    }

    return monitor;
}

/// asio's stream_descriptor closes the descriptor it is handed when it is
/// destroyed, and udev_monitor_unref closes the monitor's own descriptor -
/// handing the monitor's descriptor over directly would close it twice, and by
/// the time the second close runs that number can already have been handed out
/// again to an unrelated file on another thread. Hand out a private duplicate
/// instead: it refers to the same socket (and shares its non-blocking flag) but
/// is closed independently. F_DUPFD_CLOEXEC rather than dup() because the latter
/// would drop close-on-exec.
int dup_monitor_fd(udev_monitor& monitor, const log::Logger& logger)
{
    const int monitor_fd = udev_monitor_get_fd(&monitor);
    if (monitor_fd < 0) {
        SPDLOG_LOGGER_ERROR(logger, "udev monitor has no usable file descriptor: {}", errno_message(-monitor_fd));
        throw SerialMonitorException{};
    }

    const int owned_fd = ::fcntl(monitor_fd, F_DUPFD_CLOEXEC, 0);
    if (owned_fd < 0) {
        SPDLOG_LOGGER_ERROR(logger, "Could not duplicate the udev monitor descriptor: {}", errno_message(errno));
        throw SerialMonitorException{};
    }

    return owned_fd;
}

/// Owns the udev connection, the netlink monitor reporting tty hotplug events,
/// and the asio descriptor that makes waiting on that monitor awaitable.
///
/// Constructing this arms the monitor, which is deliberately done *before*
/// scan_connected() runs: a pod plugged in while the scan is in flight is then
/// reported by both, and open_pod() drops the duplicate - whereas arming
/// afterwards would lose it entirely.
class PodPortMonitor
{
  public:
    PodPortMonitor(cobalt::executor exec, log::Logger logger)
        : logger_{std::move(logger)}
        , context_{make_udev_context(logger_)}
        , monitor_{make_tty_monitor(*context_, logger_)}
        , descriptor_{exec}
    {
        const int owned_fd = dup_monitor_fd(*monitor_, logger_);

        boost::system::error_code ec;
        descriptor_.assign(owned_fd, ec);
        if (ec) {
            ::close(owned_fd);
            SPDLOG_LOGGER_ERROR(logger_, "Could not watch the udev monitor descriptor: {}", ec.message());
            throw SerialMonitorException{};
        }

        SPDLOG_LOGGER_INFO(logger_,
                           "Watching udev for tty devices with VID '{}', PID '{}' and USB interface '{}'.",
                           kTargetVid,
                           kTargetPid,
                           kProtocolInterfaceName);
    }

    /// Every pod protocol port that is already plugged in. Those devices never
    /// produce an "add" event, so they have to be picked up by an explicit scan.
    [[nodiscard]] std::vector<UsbTtyInfo> scan_connected() const
    {
        const UdevEnumeratePtr enumerate{udev_enumerate_new(context_.get())};
        if (!enumerate) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not create udev enumerator.");
            throw SerialInitializationException{};
        }

        if (const int rc = udev_enumerate_add_match_subsystem(enumerate.get(), "tty"); rc < 0) {
            SPDLOG_LOGGER_ERROR(logger_, "Could not restrict the udev scan to tty devices: {}", errno_message(-rc));
            throw SerialInitializationException{};
        }

        if (const int rc = udev_enumerate_scan_devices(enumerate.get()); rc < 0) {
            // Not fatal: hotplug still works, only pods that are plugged in
            // right now stay unnoticed until they are re-plugged.
            SPDLOG_LOGGER_WARN(logger_, "Could not scan for already connected tty devices: {}", errno_message(-rc));
        }

        std::vector<UsbTtyInfo> ports;
        udev_list_entry* entry = nullptr;
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate.get()))
        {
            const UdevDevicePtr dev{udev_device_new_from_syspath(context_.get(), udev_list_entry_get_name(entry))};
            if (!dev) {
                continue;
            }

            auto info = match_protocol_port(dev.get(), logger_);
            if (!info) {
                continue;
            }

            // A device udev has not finished processing may not carry its final
            // permissions yet, so opening it here could fail with EACCES for no
            // good reason. The "add" event that concludes that processing handles 
            // the opening, so skip it here and let the monitor pick it up later.
            if (udev_device_get_is_initialized(dev.get()) <= 0) {
                SPDLOG_LOGGER_DEBUG(
                    logger_, "Pod tty device '{}' is still being set up by udev; leaving it to the monitor.", info->devnode);
                continue;
            }

            ports.push_back(*std::move(info));
        }

        return ports;
    }

    /// Waits for the next tty uevent and hands back the device it concerns.
    [[nodiscard]] cobalt::promise<UdevDevicePtr> next_event()
    {
        for (;;) {
            co_await descriptor_.async_wait(asio::posix::descriptor_base::wait_read, cobalt::use_op);

            if (UdevDevicePtr dev{udev_monitor_receive_device(monitor_.get())}) {
                co_return std::move(dev);
            }

            SPDLOG_LOGGER_TRACE(logger_, "Received empty udev device event, skipping.");
        }
    }

  private:
    log::Logger logger_;
    UdevPtr context_;
    UdevMonitorPtr monitor_;
    asio::posix::stream_descriptor descriptor_;
};

/// The action of a uevent, empty when the event does not name one.
std::string_view event_action(udev_device* dev)
{
    const char* action = udev_device_get_action(dev);
    return action != nullptr ? action : std::string_view{};
}

} // namespace
#endif

boost::cobalt::generator<std::shared_ptr<IPod>> SerialPodDiscovery::discover()
{
    auto logger = cm::log::create_or_get("serial");

#ifndef __linux__
    SPDLOG_LOGGER_ERROR(logger, "USB hotplug discovery is only implemented for Linux. Aborting discovery.");
    throw SerialInitializationException{};
#else

    const auto exec = co_await cobalt::this_coro::executor;
    PodPortMonitor monitor{exec, logger};

    OpenPods open_pods;

    for (const auto& info : monitor.scan_connected()) {
        if (auto pod = open_pod(exec, open_pods, logger, info)) {
            co_yield std::move(pod);
        }
    }

    for (;;) {
        const UdevDevicePtr dev = co_await monitor.next_event();
        const auto action = event_action(dev.get());

        if (action == "remove") {
            forget_removed_port(open_pods, dev.get(), logger);
            continue;
        }

        if (action != "add") {
            continue;
        }

        const auto info = match_protocol_port(dev.get(), logger);
        if (!info) {
            continue;
        }

        if (auto pod = open_pod(exec, open_pods, logger, *info)) {
            co_yield std::move(pod);
        }
    }
#endif
}

} // namespace cm
