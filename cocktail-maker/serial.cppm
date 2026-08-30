module;
#include <boost/cobalt/generator.hpp>
#include <spdlog/common.h>

export module cm:serial;
import std;
import cm.core;
import :pod_discovery;

namespace cm {

/**
 * Discovers pods that are connected as USB CDC serial devices.
 *
 * Implemented on Linux via udev: an initial scan picks up ttys that are already plugged in, then a hotplug monitor reports later
 * arrivals and removals. A pod exposes two ports (protocol and log); only the protocol port is yielded as an @ref IPod, the log
 * port is consumed internally and streamed into a per-pod logger. Not implemented on any other platform.
 */
export class SerialPodDiscovery : public PodDiscovery
{
  public:
    /**
     * Runs discovery until the generator is destroyed.
     *
     * @throws SerialInitializationError If discovery cannot be set up at all, including unconditionally on non-Linux platforms.
     * @throws SerialMonitorError If the udev hotplug monitor fails while running.
     */
    boost::cobalt::generator<std::shared_ptr<IPod>> discover() override;
};

/** Thrown when serial pod discovery cannot be set up in the first place, e.g. because udev itself could not be reached or the
 * platform has no hotplug support. */
export class SerialInitializationError : public std::runtime_error
{
  public:
    /** @param cause Short description of what failed, embedded in `what()`. */
    explicit SerialInitializationError(std::string_view cause)
        : std::runtime_error{std::format("serial discovery could not be initialized: {}", cause)}
    {
    }
};

/** Thrown when the udev hotplug monitor that discovery relies on fails after it has already started running. */
export class SerialMonitorError : public std::runtime_error
{
  public:
    /** @param cause Short description of what failed, embedded in `what()`. */
    explicit SerialMonitorError(std::string_view cause)
        : std::runtime_error{std::format("serial hotplug monitor failed: {}", cause)}
    {
    }
};

namespace serial_detail {

export constexpr std::string_view kTargetVid = "2fe3"; // TODO: Replace with Vendor ID
export constexpr std::string_view kTargetPid = "0001"; // TODO: Replace with Product ID

/// USB interface name of the pod's protocol port, checked before falling back to @ref kProtocolInterfaceNumber.
export constexpr std::string_view kProtocolInterfaceName = "Cocktailmaker Pod Protocol";
/// USB interface name of the pod's log port, checked before falling back to @ref kLogInterfaceNumber.
export constexpr std::string_view kLogInterfaceName = "Cocktailmaker Pod Log";

/// `bInterfaceNumber` of the protocol port, used only when the device does not report an interface name.
export constexpr std::string_view kProtocolInterfaceNumber = "02";
/// `bInterfaceNumber` of the log port, used only when the device does not report an interface name.
export constexpr std::string_view kLogInterfaceNumber = "00";

/// Which of a pod's two serial functions a tty belongs to; see @ref port_role.
export enum class PortRole
{
    protocol,
    log,
};

/// Identifies one physical pod, stable across whichever of its two ports it was seen on; see @ref pod_identity.
export using PodHardwareId = strong_type<std::string, struct PodHardwareIdTag, Comparable, Hashable, Formattable>;

/**
 * The identifying attributes of one tty, read from udev/sysfs.
 *
 * `device_serial`, `interface_name` and `interface_number` are absent when the underlying device does not report them (e.g. a tty
 * that is not a USB CDC ACM device at all).
 */
export struct UsbTtyInfo
{
    std::string devnode;
    std::string vendor_id;
    std::string product_id;
    std::optional<std::string> device_serial;
    std::optional<std::string> interface_name;
    std::optional<std::string> interface_number;
};

/// Returns whether `info`'s VID/PID match a cocktail-maker pod (@ref kTargetVid, @ref kTargetPid).
export bool is_pod_device(const UsbTtyInfo& info)
{
    return info.vendor_id == kTargetVid && info.product_id == kTargetPid;
}

/**
 * Determines which of a pod's two ports `info` describes.
 *
 * The interface name is authoritative when present: an interface number that would otherwise say something different is ignored,
 * and a name that matches neither known port does not fall back to the number at all. The number is only consulted when no name
 * was reported.
 *
 * @returns `std::nullopt` if `info` matches neither the protocol nor the log port by any of the available attributes.
 */
export std::optional<PortRole> port_role(const UsbTtyInfo& info)
{
    if (info.interface_name) {
        if (*info.interface_name == kProtocolInterfaceName) {
            return PortRole::protocol;
        }
        if (*info.interface_name == kLogInterfaceName) {
            return PortRole::log;
        }
        return std::nullopt;
    }

    if (info.interface_number == kProtocolInterfaceNumber) {
        return PortRole::protocol;
    }
    if (info.interface_number == kLogInterfaceNumber) {
        return PortRole::log;
    }
    return std::nullopt;
}

/**
 * Derives the stable identity of the pod behind `info`.
 *
 * Prefers the device's USB serial number; falls back to the devnode when no serial is reported.
 * A devnode-derived identity is not stable across replugging, since the same pod can be assigned a different devnode.
 */
export PodHardwareId pod_identity(const UsbTtyInfo& info)
{
    return PodHardwareId{info.device_serial.value_or(info.devnode)};
}

/**
 * Tracks the one devnode currently open for each pod identity, so the same physical pod is never opened twice at once through its
 * two ports (or through several devnodes at once).
 *
 * Reserving an identity is split into @ref claim, which stakes the reservation out immediately so a concurrent second attempt is
 * refused, and @ref Claim::commit, which makes it permanent. A claim that is dropped without being committed (e.g. because
 * opening the port then failed) releases the reservation again, so a later attempt for the same identity can retry.
 */
export class PortTable
{
  public:
    /**
     * An in-progress reservation for one pod identity.
     *
     * Move-only. Releases the reservation on destruction unless @ref commit has been called.
     */
    class Claim
    {
      public:
        Claim(const Claim&) = delete;
        Claim& operator=(const Claim&) = delete;

        Claim(Claim&& other) noexcept
            : table_{std::exchange(other.table_, nullptr)}
            , id_{std::move(other.id_)}
        {
        }

        Claim& operator=(Claim&& other) noexcept
        {
            std::swap(table_, other.table_);
            std::swap(id_, other.id_);
            return *this;
        }

        ~Claim()
        {
            if (table_ != nullptr) {
                table_->entries_.erase(id_);
            }
        }

        /// Makes the reservation permanent; the destructor no longer releases it.
        void commit() noexcept
        {
            table_ = nullptr;
        }

      private:
        friend class PortTable;

        Claim(PortTable& table, PodHardwareId id)
            : table_{&table}
            , id_{std::move(id)}
        {
        }

        PortTable* table_;
        PodHardwareId id_;
    };

    /**
     * Reserves `id` against `devnode`.
     *
     * @returns `std::nullopt` if `id` already has an entry (committed or not); otherwise a @ref Claim that must be committed to
     * keep the reservation.
     */
    [[nodiscard]] std::optional<Claim> claim(const PodHardwareId& id, std::string devnode)
    {
        const auto [entry, is_new] = entries_.try_emplace(id, std::move(devnode));
        if (!is_new) {
            return std::nullopt;
        }
        return Claim{*this, id};
    }

    /**
     * Removes the committed entry whose devnode is `devnode`, if any.
     *
     * @returns The identity that was removed, or `std::nullopt` if no entry has that devnode.
     */
    [[nodiscard]] std::optional<PodHardwareId> release_by_devnode(std::string_view devnode)
    {
        const auto entry = std::ranges::find_if(entries_, [devnode](const auto& e) { return e.second == devnode; });
        if (entry == entries_.end()) {
            return std::nullopt;
        }

        auto id = entry->first;
        entries_.erase(entry);
        return id;
    }

    /// Returns the devnode currently reserved for `id`, if any.
    [[nodiscard]] std::optional<std::string_view> devnode_of(const PodHardwareId& id) const
    {
        const auto entry = entries_.find(id);
        if (entry == entries_.end()) {
            return std::nullopt;
        }
        return entry->second;
    }

  private:
    std::map<PodHardwareId, std::string> entries_;
};

/// Maps the Zephyr log-level tags pod firmware embeds in its boot log to the equivalent spdlog level.
export constexpr std::array<std::pair<std::string_view, log::Level>, 4> kZephyrLevelTags{{
    {"<err>", log::level::error},
    {"<wrn>", log::level::warn},
    {"<inf>", log::level::info},
    {"<dbg>", log::level::debug},
}};

/// Bytes an unterminated line may accumulate in @ref LineAssembler before it is force-flushed as a line.
export constexpr std::size_t kMaxLogLine = 4096;

/**
 * Removes ANSI CSI escape sequences (`ESC [ ... final-byte`) from `line`.
 *
 * A bare `ESC` not followed by `[` is dropped on its own without affecting the rest of the line.
 * An `ESC [` introducer that never reaches a valid final byte (0x40-0x7e) before the end of the string is treated as unterminated
 * and discards everything from the escape to the end of the line, not just the malformed sequence itself.
 */
export std::string strip_ansi(std::string_view line)
{
    if (!line.contains('\x1b')) {
        return std::string{line};
    }

    const auto is_parameter_byte = [](char c) { return c >= '\x20' && c <= '\x3f'; };
    const auto is_final_byte = [](char c) { return c >= '\x40' && c <= '\x7e'; };

    std::string out;
    out.reserve(line.size());

    for (std::size_t i = 0; i < line.size();) {
        if (line[i] != '\x1b') {
            out.push_back(line[i++]);
            continue;
        }

        if (i + 1 >= line.size() || line[i + 1] != '[') {
            ++i;
            continue;
        }

        auto end = i + 2;
        while (end < line.size() && is_parameter_byte(line[end])) {
            ++end;
        }

        i = end < line.size() && is_final_byte(line[end]) ? end + 1 : line.size();
    }

    return out;
}

/// One line of pod firmware output, split into its log level and message text.
export struct FirmwareLine
{
    log::Level level{log::level::info};
    std::string text;
};

/**
 * Parses one line of raw pod firmware output.
 *
 * Strips ANSI escape sequences first (see @ref strip_ansi), then looks for a Zephyr level tag (see @ref kZephyrLevelTags)
 * anywhere in the remaining text and removes it along with one following space. `raw` should not include the trailing newline.
 *
 * @returns A line at `log::level::info` if no known tag is found.
 */
export FirmwareLine parse_firmware_line(std::string_view raw)
{
    FirmwareLine parsed{.level = log::level::info, .text = strip_ansi(raw)};

    for (const auto& [tag, level] : kZephyrLevelTags) {
        const auto pos = parsed.text.find(tag);
        if (pos == std::string::npos) {
            continue;
        }

        parsed.level = level;
        parsed.text.erase(pos, tag.size());
        if (pos < parsed.text.size() && parsed.text[pos] == ' ') {
            parsed.text.erase(pos, 1);
        }
        break;
    }

    return parsed;
}

/**
 * Reassembles a byte stream that may split lines at arbitrary points into whole `\n`-terminated lines.
 *
 * A trailing, not yet newline-terminated fragment is held back across calls to @ref feed until either its newline arrives or it
 * grows past the configured limit, at which point it is flushed as a line on its own to bound how much unterminated data can
 * accumulate from a firmware that never sends a newline.
 */
export class LineAssembler
{
  public:
    /// @param max_line Bytes an unterminated line may accumulate before being force-flushed.
    explicit LineAssembler(std::size_t max_line = kMaxLogLine)
        : max_line_{max_line}
    {
    }

    /**
     * Feeds `chunk` into the assembler, calling `on_line` once per complete line found (including a forced flush past the size
     * limit), in order, with any trailing `\r` stripped.
     *
     * @param on_line Called synchronously from within this call, with a `std::string_view` into this assembler's internal buffer.
     * The view is only valid for the duration of that call and must not be retained past it.
     */
    template <std::invocable<std::string_view> Fn>
    void feed(std::string_view chunk, Fn on_line)
    {
        pending_.append(chunk);

        std::size_t consumed = 0;
        for (auto nl = pending_.find('\n'); nl != std::string::npos; nl = pending_.find('\n', consumed)) {
            std::string_view line{pending_};
            line = line.substr(consumed, nl - consumed);
            if (line.ends_with('\r')) {
                line.remove_suffix(1);
            }

            on_line(line);
            consumed = nl + 1;
        }
        pending_.erase(0, consumed);

        if (pending_.size() > max_line_) {
            on_line(std::string_view{pending_});
            pending_.clear();
        }
    }

  private:
    std::size_t max_line_;
    std::string pending_;
};

} // namespace serial_detail
} // namespace cm
