#include <catch2/catch_test_macros.hpp>
import std;
import cm;
import cm.core;

using namespace cm::serial_detail;
namespace log = cm::log;

namespace {

UsbTtyInfo pod_tty(std::string devnode = "/dev/ttyACM0")
{
    return UsbTtyInfo{
        .devnode = std::move(devnode),
        .vendor_id = std::string{kTargetVid},
        .product_id = std::string{kTargetPid},
    };
}

std::vector<std::string> collect(LineAssembler& lines, std::string_view chunk)
{
    std::vector<std::string> out;
    lines.feed(chunk, [&out](std::string_view line) { out.emplace_back(line); });
    return out;
}

} // namespace

TEST_CASE("is_pod_device matches on VID and PID", "[serial]")
{
    REQUIRE(is_pod_device(pod_tty()));

    auto foreign = pod_tty();
    foreign.vendor_id = "1234";
    REQUIRE_FALSE(is_pod_device(foreign));

    foreign = pod_tty();
    foreign.product_id = "9999";
    REQUIRE_FALSE(is_pod_device(foreign));
}

TEST_CASE("port_role prefers the interface name", "[serial]")
{
    auto info = pod_tty();

    info.interface_name = std::string{kProtocolInterfaceName};
    REQUIRE(port_role(info) == PortRole::protocol);

    info.interface_name = std::string{kLogInterfaceName};
    REQUIRE(port_role(info) == PortRole::log);

    // A known name must win over a number that says otherwise.
    info.interface_name = std::string{kLogInterfaceName};
    info.interface_number = std::string{kProtocolInterfaceNumber};
    REQUIRE(port_role(info) == PortRole::log);

    // An unknown name must not fall through to the number.
    info.interface_name = "Some Other Function";
    info.interface_number = std::string{kProtocolInterfaceNumber};
    REQUIRE_FALSE(port_role(info).has_value());
}

TEST_CASE("port_role falls back to the interface number", "[serial]")
{
    auto info = pod_tty();

    info.interface_number = std::string{kProtocolInterfaceNumber};
    REQUIRE(port_role(info) == PortRole::protocol);

    info.interface_number = std::string{kLogInterfaceNumber};
    REQUIRE(port_role(info) == PortRole::log);

    info.interface_number = "07";
    REQUIRE_FALSE(port_role(info).has_value());

    info.interface_number.reset();
    REQUIRE_FALSE(port_role(info).has_value());
}

TEST_CASE("pod_identity prefers the USB serial", "[serial]")
{
    auto info = pod_tty("/dev/ttyACM3");
    REQUIRE(pod_identity(info) == PodHardwareId{"/dev/ttyACM3"});

    info.device_serial = "E6614C311B8C2F35";
    REQUIRE(pod_identity(info) == PodHardwareId{"E6614C311B8C2F35"});
}

TEST_CASE("strip_ansi removes CSI sequences", "[serial]")
{
    REQUIRE(strip_ansi("plain text") == "plain text");
    REQUIRE(strip_ansi("\x1b[1;32m<inf> boot: up\x1b[0m") == "<inf> boot: up");

    // Not an SGR sequence, but still a sequence.
    REQUIRE(strip_ansi("a\x1b[Kb") == "ab");

    // A bare ESC is dropped, the text around it is not.
    REQUIRE(strip_ansi("a\x1b"
                       "b") == "ab");

    // An unterminated sequence used to discard the rest of the line.
    REQUIRE(strip_ansi("keep me\x1b[") == "keep me");
    REQUIRE(strip_ansi("keep me too\x1b[1;32mand this") == "keep me tooand this");
}

TEST_CASE("parse_firmware_line extracts the Zephyr level", "[serial]")
{
    REQUIRE(parse_firmware_line("<err> pod: broke").level == log::level::error);
    REQUIRE(parse_firmware_line("<wrn> pod: hmm").level == log::level::warn);
    REQUIRE(parse_firmware_line("<inf> pod: fine").level == log::level::info);
    REQUIRE(parse_firmware_line("<dbg> pod: chatty").level == log::level::debug);

    // The tag and the space after it are removed; the rest is untouched.
    REQUIRE(parse_firmware_line("[00:00:01.234,000] <inf> pod: fine").text == "[00:00:01.234,000] pod: fine");

    const auto untagged = parse_firmware_line("no tag here");
    REQUIRE(untagged.level == log::level::info);
    REQUIRE(untagged.text == "no tag here");

    REQUIRE(parse_firmware_line("\x1b[0m").text.empty());
}

TEST_CASE("LineAssembler reassembles chopped chunks", "[serial]")
{
    LineAssembler lines;

    REQUIRE(collect(lines, "ready\nstea") == std::vector<std::string>{"ready"});
    REQUIRE(collect(lines, "dy\n") == std::vector<std::string>{"steady"});
    REQUIRE(collect(lines, "a\nb\nc\n") == std::vector<std::string>{"a", "b", "c"});

    // Windows-style endings and empty lines.
    REQUIRE(collect(lines, "crlf\r\n\n") == std::vector<std::string>{"crlf", ""});

    // An unterminated tail is held back until its newline arrives.
    REQUIRE(collect(lines, "tail").empty());
    REQUIRE(collect(lines, "\n") == std::vector<std::string>{"tail"});
}

TEST_CASE("LineAssembler flushes a line that never ends", "[serial]")
{
    LineAssembler lines{8};

    REQUIRE(collect(lines, "1234567").empty());

    const auto flushed = collect(lines, "89");
    REQUIRE(flushed == std::vector<std::string>{"123456789"});

    // The buffer was cleared, so the next line starts fresh.
    REQUIRE(collect(lines, "next\n") == std::vector<std::string>{"next"});
}

TEST_CASE("PortTable keeps one port per pod", "[serial]")
{
    const PodHardwareId pod{"E6614C311B8C2F35"};
    PortTable table;

    auto first = table.claim(pod, "/dev/ttyACM0");
    REQUIRE(first.has_value());
    first->commit();
    REQUIRE(table.devnode_of(pod) == "/dev/ttyACM0");

    // The same pod showing up through a second devnode is refused.
    REQUIRE_FALSE(table.claim(pod, "/dev/ttyACM4").has_value());
    REQUIRE(table.devnode_of(pod) == "/dev/ttyACM0");
}

TEST_CASE("PortTable releases an uncommitted claim", "[serial]")
{
    const PodHardwareId pod{"E6614C311B8C2F35"};
    PortTable table;

    {
        auto claim = table.claim(pod, "/dev/ttyACM0");
        REQUIRE(claim.has_value());
    }

    // A port that failed to open must not leave the pod marked as taken.
    REQUIRE_FALSE(table.devnode_of(pod).has_value());

    auto retry = table.claim(pod, "/dev/ttyACM0");
    REQUIRE(retry.has_value());
}

TEST_CASE("PortTable releases by devnode", "[serial]")
{
    const PodHardwareId first{"pod-a"};
    const PodHardwareId second{"pod-b"};
    PortTable table;

    table.claim(first, "/dev/ttyACM0")->commit();
    table.claim(second, "/dev/ttyACM2")->commit();

    REQUIRE(table.release_by_devnode("/dev/ttyACM2") == second);
    REQUIRE_FALSE(table.devnode_of(second).has_value());
    REQUIRE(table.devnode_of(first) == "/dev/ttyACM0");

    // Removing an unknown devnode is not an error.
    REQUIRE_FALSE(table.release_by_devnode("/dev/ttyACM2").has_value());
    REQUIRE_FALSE(table.release_by_devnode("/dev/ttyUSB0").has_value());
}
