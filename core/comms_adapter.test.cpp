#include <catch2/catch_test_macros.hpp>
#include <comms/ErrorStatus.h>
#include <proto/MsgId.h>

import std;
import cm.core;

TEST_CASE("comms::ErrorStatus formats to the expected string", "[comms]")
{
    using comms::ErrorStatus;

    REQUIRE(std::format("{}", ErrorStatus::Success) == "Success");
    REQUIRE(std::format("{}", ErrorStatus::UpdateRequired) == "UpdateRequired");
    REQUIRE(std::format("{}", ErrorStatus::NotEnoughData) == "NotEnoughData");
    REQUIRE(std::format("{}", ErrorStatus::ProtocolError) == "ProtocolError");
    REQUIRE(std::format("{}", ErrorStatus::BufferOverflow) == "BufferOverflow");
    REQUIRE(std::format("{}", ErrorStatus::InvalidMsgId) == "InvalidMsgId");
    REQUIRE(std::format("{}", ErrorStatus::InvalidMsgData) == "InvalidMsgData");
    REQUIRE(std::format("{}", ErrorStatus::MsgAllocFailure) == "MsgAllocFailure");
    REQUIRE(std::format("{}", ErrorStatus::NotSupported) == "NotSupported");
}

TEST_CASE("comms::ErrorStatus formatter handles out-of-range values", "[comms]")
{
    using comms::ErrorStatus;

    const auto invalid = static_cast<ErrorStatus>(999);
    REQUIRE(std::format("{}", invalid) == "Unknown");
}

TEST_CASE("proto::MsgId formats mapped names")
{
    using proto::MsgId;

    REQUIRE(std::format("{}", MsgId::MsgId_Ping) == "Ping Server Message");
    REQUIRE(std::format("{}", MsgId::MsgId_Pong) == "Pong Client Message");
    REQUIRE(std::format("{}", MsgId::MsgId_Ack) == "Ack");
    REQUIRE(std::format("{}", MsgId::MsgId_DeviceInfoRequest) == "Device Info Request");
    REQUIRE(std::format("{}", MsgId::MsgId_DeviceInfoResponse) == "Device Info Response");
    REQUIRE(std::format("{}", MsgId::MsgId_EmergencyStop) == "Emergency Stop");
}

TEST_CASE("proto::MsgId formats unknown values as numbers")
{
    using proto::MsgId;
    constexpr auto kMax = std::numeric_limits<std::underlying_type_t<MsgId>>::max();

    REQUIRE(std::format("{}", static_cast<MsgId>(kMax)) == std::to_string(kMax));
}
