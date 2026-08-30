module;
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
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
using namespace serial_detail;

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

/// Closes a descriptor until it is release()d to its final owner.
class UniqueFd
{
  public:
    explicit UniqueFd(int fd) noexcept
        : fd_{fd}
    {
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept
        : fd_{std::exchange(other.fd_, -1)}
    {
    }

    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        std::swap(fd_, other.fd_);
        return *this;
    }

    ~UniqueFd()
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    [[nodiscard]] int get() const noexcept
    {
        return fd_;
    }

    /// Gives up ownership once someone else has taken the descriptor over.
    void disown() noexcept
    {
        fd_ = -1;
    }

  private:
    int fd_;
};

/// libudev reports failures as negated errno values rather than through errno.
std::string errno_message(int error_number)
{
    return std::error_code{error_number, std::generic_category()}.message();
}

/// The returned view is owned by `dev` and stays valid as long as it does.
std::optional<std::string_view> sysattr(udev_device* dev, const char* name)
{
    const char* value = udev_device_get_sysattr_value(dev, name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string_view{value};
}

std::optional<std::string_view> devnode_of(udev_device* dev)
{
    const char* devnode = udev_device_get_devnode(dev);
    if (devnode == nullptr) {
        return std::nullopt;
    }
    return std::string_view{devnode};
}

/// Collects the identifying attributes of a tty that belongs to a USB device.
/// Returns nullopt for ttys that are not USB-backed at all (serial-over-PCI, pseudo terminals, ...), which is the bulk of what
/// the monitor reports.
///
/// Both parent lookups return references owned by `dev`, they must not be unref'd.
std::optional<UsbTtyInfo> describe_usb_tty(udev_device* dev)
{
    const auto to_string = [](std::string_view value) { return std::string{value}; };

    const auto devnode = devnode_of(dev);
    if (!devnode) {
        return std::nullopt;
    }

    udev_device* usb_device = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    if (usb_device == nullptr) {
        return std::nullopt;
    }

    udev_device* usb_interface = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_interface");
    if (usb_interface == nullptr) {
        return std::nullopt;
    }

    // Read straight from sysfs rather than via ID_VENDOR_ID/ID_MODEL_ID, which only exist once udev's usb_id builtin has run for
    // the device.
    const auto vendor_id = sysattr(usb_device, "idVendor");
    const auto product_id = sysattr(usb_device, "idProduct");
    if (!vendor_id || !product_id) {
        return std::nullopt;
    }

    return UsbTtyInfo{
        .devnode = std::string{*devnode},
        .vendor_id = std::string{*vendor_id},
        .product_id = std::string{*product_id},
        .device_serial = sysattr(usb_device, "serial").transform(to_string),
        .interface_name = sysattr(usb_interface, "interface").transform(to_string),
        .interface_number = sysattr(usb_interface, "bInterfaceNumber").transform(to_string),
    };
}

struct PodPort
{
    PortRole role;
    UsbTtyInfo info;
};

std::optional<PodPort> match_pod_port(udev_device* dev, const log::Logger& logger)
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

    const auto role = port_role(*info);
    if (!role) {
        SPDLOG_LOGGER_DEBUG(logger,
                            "Ignoring pod tty device '{}': USB interface '{}' (#{}) is not a known port.",
                            info->devnode,
                            info->interface_name.value_or("<unnamed>"),
                            info->interface_number.value_or("?"));
        return std::nullopt;
    }

    return PodPort{.role = *role, .info = *std::move(info)};
}

constexpr unsigned int kPodBaudRate = 115200;

/// Opens `devnode` and puts it into the 8N1 line discipline the pod expects.
/// asio opens with O_NONBLOCK and applies the options with TCSANOW, so nothing here waits on a modem line or on output draining.
std::expected<asio::serial_port, boost::system::error_code> open_and_configure(cobalt::executor exec, const std::string& devnode)
{
    boost::system::error_code ec;
    asio::serial_port port{std::move(exec)};
    if (port.open(devnode, ec)) {
        return std::unexpected{ec};
    }

    using serial_option = asio::serial_port_base;
    const auto apply = [&](const auto& option) {
        if (!ec) {
            port.set_option(option, ec);
        }
    };

    apply(serial_option::baud_rate(kPodBaudRate));
    apply(serial_option::character_size(8));
    apply(serial_option::parity(serial_option::parity::none));
    apply(serial_option::stop_bits(serial_option::stop_bits::one));
    // Spelled out rather than inherited: asio's defaults leave CRTSCTS alone, and hardware flow control would stall every write
    // until a modem line the pod does not drive gets asserted.
    apply(serial_option::flow_control(serial_option::flow_control::none));

    if (ec) {
        return std::unexpected{ec};
    }

    return port;
}

cobalt::task<void> stream_pod_log(asio::serial_port port, log::Logger logger)
{
    std::array<char, 1024> chunk{};
    LineAssembler lines;

    const auto emit = [&logger](std::string_view raw) {
        const auto line = parse_firmware_line(raw);
        if (!line.text.empty()) {
            logger->log(line.level, "{}", line.text);
        }
    };

    for (;;) {
        auto [ec, bytes_read] = co_await port.async_read_some(asio::buffer(chunk), asio::as_tuple(cobalt::use_op));
        if (ec) {
            SPDLOG_LOGGER_DEBUG(logger, "Pod log stream ended: {}", ec.message());
            co_return;
        }

        lines.feed(std::string_view{chunk.data(), bytes_read}, emit);
    }
}

struct DiscoveryState
{
    cobalt::executor exec;
    log::Logger logger;
    PortTable pods;
    PortTable logs;
};

/// Opens a pod's protocol port, or returns nullptr if it should not or cannot be opened.
std::shared_ptr<IPod> open_pod(DiscoveryState& state, const UsbTtyInfo& info)
{
    const auto identity = pod_identity(info);

    // One session per physical pod, keyed on the pod rather than the devnode: the same pod can be reachable through several
    // devnodes at once (a USB/IP export on two vhci ports, an attachment that is not torn down yet), and a second session would
    // put two competing command streams on one machine.
    auto claim = state.pods.claim(identity, info.devnode);
    if (!claim) {
        SPDLOG_LOGGER_DEBUG(state.logger,
                            "Pod '{}' is already open on '{}', ignoring its second port '{}'.",
                            identity,
                            state.pods.devnode_of(identity).value_or("?"),
                            info.devnode);
        return nullptr;
    }

    SPDLOG_LOGGER_INFO(state.logger,
                       "Matching serial pod found at '{}' (interface '{}' #{}, device serial '{}').",
                       info.devnode,
                       info.interface_name.value_or("<unnamed>"),
                       info.interface_number.value_or("?"),
                       info.device_serial.value_or("<none>"));

    // A port that refuses to open is reported and skipped rather than failing discovery: discovery is the only thing that ever
    // hands pods to the station, so ending it over one unusable port would take every other pod down with it. The claim is not
    // committed, so a later "add" event can retry.
    auto port = open_and_configure(state.exec, info.devnode);
    if (!port) {
        SPDLOG_LOGGER_ERROR(state.logger,
                            "Failed to open serial port '{}' ({}); skipping it: {}",
                            info.devnode,
                            port.error().value(),
                            port.error().message());
        return nullptr;
    }

    claim->commit();
    SPDLOG_LOGGER_DEBUG(state.logger, "Opened serial port '{}'.", info.devnode);

    return std::make_shared<Pod>(std::make_unique<SocketIoStream<asio::serial_port>>(*std::move(port)));
}

/// Opens a pod's log port and pumps its output into a per-pod spdlog logger.
void start_log_stream(DiscoveryState& state, const UsbTtyInfo& info)
{
    const auto identity = pod_identity(info);

    auto claim = state.logs.claim(identity, info.devnode);
    if (!claim) {
        SPDLOG_LOGGER_DEBUG(state.logger,
                            "Log of pod '{}' already streams from '{}', ignoring '{}'.",
                            identity,
                            state.logs.devnode_of(identity).value_or("?"),
                            info.devnode);
        return;
    }

    auto port = open_and_configure(state.exec, info.devnode);
    if (!port) {
        SPDLOG_LOGGER_ERROR(state.logger,
                            "Failed to open pod log port '{}' ({}); skipping it: {}",
                            info.devnode,
                            port.error().value(),
                            port.error().message());
        return;
    }

    claim->commit();

    auto log_name = std::format("pod_log_{}", identity);
    SPDLOG_LOGGER_INFO(state.logger, "Streaming pod log from '{}' into logger '{}'.", info.devnode, log_name);

    // Detached: the log tty comes and goes on its own, independently of whether the pod's protocol session is up.
    cobalt::spawn(state.exec, stream_pod_log(*std::move(port), log::create_or_get(std::move(log_name))), boost::asio::detached);
}

/// Dispatches one newly seen port to its role.
/// Returns a pod session only for a protocol port that was opened just now.
std::shared_ptr<IPod> handle_port(DiscoveryState& state, const PodPort& port)
{
    if (port.role == PortRole::log) {
        start_log_stream(state, port.info);
        return nullptr;
    }

    return open_pod(state, port.info);
}

/// Drops the bookkeeping entry of a tty that has just disappeared, so the pod behind it can be opened again when it comes back.
///
/// Unplugging is not reported through the generator: the pod's session ends on its own once reads on the vanished port start
/// failing, and the registry entry is dropped with it (see run_pod()). The log stream ends the same way.
void forget_removed_port(DiscoveryState& state, udev_device* dev)
{
    const auto devnode = devnode_of(dev);
    if (!devnode) {
        return;
    }

    // Only one of the two tables can hold the vanished devnode.
    for (auto* table : {&state.pods, &state.logs}) {
        if (const auto identity = table->release_by_devnode(*devnode)) {
            SPDLOG_LOGGER_INFO(state.logger, "Port '{}' of pod '{}' was removed.", *devnode, *identity);
        }
    }
}

UdevPtr make_udev_context()
{
    UdevPtr context{udev_new()};
    if (!context) {
        throw SerialInitializationError{"could not create a udev context"};
    }

    return context;
}

UdevMonitorPtr make_tty_monitor(udev& context)
{
    UdevMonitorPtr monitor{udev_monitor_new_from_netlink(&context, "udev")};
    if (!monitor) {
        throw SerialMonitorError{"could not create a netlink monitor"};
    }

    // Filter only ttys
    if (const int rc = udev_monitor_filter_add_match_subsystem_devtype(monitor.get(), "tty", nullptr); rc < 0) {
        throw SerialMonitorError{std::format("could not install the 'tty' subsystem filter: {}", errno_message(-rc))};
    }

    if (const int rc = udev_monitor_enable_receiving(monitor.get()); rc < 0) {
        throw SerialMonitorError{std::format("could not start receiving events: {}", errno_message(-rc))};
    }

    return monitor;
}

/// asio's stream_descriptor and udev_monitor_unref would each close the monitor's descriptor, and by the time the second close
/// runs that number can already have been reused.
/// Hand out a private duplicate instead. F_DUPFD_CLOEXEC rather than dup(), which would drop close-on-exec.
UniqueFd dup_monitor_fd(udev_monitor& monitor)
{
    const int monitor_fd = udev_monitor_get_fd(&monitor);
    if (monitor_fd < 0) {
        throw SerialMonitorError{std::format("monitor has no usable descriptor: {}", errno_message(-monitor_fd))};
    }

    UniqueFd owned_fd{::fcntl(monitor_fd, F_DUPFD_CLOEXEC, 0)};
    if (owned_fd.get() < 0) {
        throw SerialMonitorError{std::format("could not duplicate the monitor descriptor: {}", errno_message(errno))};
    }

    return owned_fd;
}

class PodPortMonitor
{
  public:
    PodPortMonitor(cobalt::executor exec, log::Logger logger)
        : logger_{std::move(logger)}
        , context_{make_udev_context()}
        , monitor_{make_tty_monitor(*context_)}
        , descriptor_{exec}
    {
        UniqueFd owned_fd = dup_monitor_fd(*monitor_);

        boost::system::error_code ec;
        if (descriptor_.assign(owned_fd.get(), ec)) {
            throw SerialMonitorError{std::format("could not watch the monitor descriptor: {}", ec.message())};
        }
        owned_fd.disown();

        SPDLOG_LOGGER_INFO(logger_,
                           "Watching udev for tty devices with VID '{}', PID '{}' and USB interface '{}' or '{}'.",
                           kTargetVid,
                           kTargetPid,
                           kProtocolInterfaceName,
                           kLogInterfaceName);
    }

    /// Connected pods never produce an "add" event, so they have to be picked up by an explicit scan.
    [[nodiscard]] std::vector<PodPort> scan_connected() const
    {
        const UdevEnumeratePtr enumerate{udev_enumerate_new(context_.get())};
        if (!enumerate) {
            throw SerialInitializationError{"could not create a udev enumerator"};
        }

        if (const int rc = udev_enumerate_add_match_subsystem(enumerate.get(), "tty"); rc < 0) {
            throw SerialInitializationError{std::format("could not restrict the scan to tty devices: {}", errno_message(-rc))};
        }

        if (const int rc = udev_enumerate_scan_devices(enumerate.get()); rc < 0) {
            // Not fatal: hotplug still works, only pods that are plugged in right now stay unnoticed until they are re-plugged.
            SPDLOG_LOGGER_WARN(logger_, "Could not scan for already connected tty devices: {}", errno_message(-rc));
        }

        std::vector<PodPort> ports;
        udev_list_entry* entry = nullptr;
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate.get()))
        {
            const UdevDevicePtr dev{udev_device_new_from_syspath(context_.get(), udev_list_entry_get_name(entry))};
            if (!dev) {
                continue;
            }

            auto port = match_pod_port(dev.get(), logger_);
            if (!port) {
                continue;
            }

            // A device udev has not finished processing may not carry its final permissions yet, so opening it here could fail
            // with EACCES. The concluding "add" event opens it instead.
            if (udev_device_get_is_initialized(dev.get()) <= 0) {
                SPDLOG_LOGGER_DEBUG(
                    logger_, "Pod tty device '{}' is still being set up by udev; leaving it to the monitor.", port->info.devnode);
                continue;
            }

            ports.push_back(*std::move(port));
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

enum class UdevAction
{
    add,
    remove,
    other,
};

UdevAction event_action(udev_device* dev)
{
    const char* action = udev_device_get_action(dev);
    if (action == nullptr) {
        return UdevAction::other;
    }

    const std::string_view name{action};
    if (name == "add") {
        return UdevAction::add;
    }
    if (name == "remove") {
        return UdevAction::remove;
    }
    return UdevAction::other;
}

} // namespace
#endif

boost::cobalt::generator<std::shared_ptr<IPod>> SerialPodDiscovery::discover()
{
    auto logger = cm::log::create_or_get("serial");

#ifndef __linux__
    throw SerialInitializationError{"USB hotplug discovery is only implemented for Linux"};
#else

    DiscoveryState state{.exec = co_await cobalt::this_coro::executor, .logger = std::move(logger)};
    PodPortMonitor monitor{state.exec, state.logger};

    for (const auto& port : monitor.scan_connected()) {
        if (auto pod = handle_port(state, port)) {
            co_yield std::move(pod);
        }
    }

    for (;;) {
        const UdevDevicePtr dev = co_await monitor.next_event();

        switch (event_action(dev.get())) {
        case UdevAction::remove:
            forget_removed_port(state, dev.get());
            break;

        case UdevAction::add:
            if (const auto port = match_pod_port(dev.get(), state.logger)) {
                if (auto pod = handle_port(state, *port)) {
                    co_yield std::move(pod);
                }
            }
            break;

        case UdevAction::other:
            break;
        }
    }
#endif
}

} // namespace cm
