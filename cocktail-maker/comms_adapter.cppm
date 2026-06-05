module;
#include <comms/ErrorStatus.h>
#include <proto/MsgId.h>
#include <proto/field/MsgIdCommon.h>

export module cm:comms_adapter;
import std;

namespace {
using namespace comms;

constexpr std::string_view to_string_view(ErrorStatus status) noexcept
{
    constexpr std::array<std::string_view, static_cast<std::size_t>(ErrorStatus::NumOfErrorStatuses)> names{
        "Success",
        "UpdateRequired",
        "NotEnoughData",
        "ProtocolError",
        "BufferOverflow",
        "InvalidMsgId",
        "InvalidMsgData",
        "MsgAllocFailure",
        "NotSupported",
    };

    const auto idx = static_cast<std::size_t>(status);
    return idx < names.size() ? names[idx] : "Unknown";
}

} // namespace

namespace std {

export template <>
struct formatter<comms::ErrorStatus, char> : formatter<std::string_view>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return formatter<std::string_view>::parse(ctx);
    }

    auto format(comms::ErrorStatus status, format_context& ctx) const
    {
        return formatter<std::string_view>::format(to_string_view(status), ctx);
    }
};

export template <>
struct formatter<proto::MsgId, char> : formatter<std::string_view>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return formatter<std::string_view>::parse(ctx);
    }

    auto format(proto::MsgId value, format_context& ctx) const
    {
        const auto map_info = proto::field::MsgIdCommon::valueNamesMap();
        const auto* beg = map_info.first;
        const auto* end = beg + map_info.second;

        const auto* const it = std::find_if(beg, end, [value](const auto& entry) { return entry.first == value; });

        if (it != end) {
            return formatter<std::string_view>::format(it->second, ctx);
        }

        using Underlying = std::underlying_type_t<proto::MsgId>;
        return formatter<Underlying, char>{}.format(static_cast<Underlying>(value), ctx);
    }
};

} // namespace std