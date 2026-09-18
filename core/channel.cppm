module;
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/system/system_error.hpp>
#include <exec/asio/use_sender.hpp>
#include <stdexec/execution.hpp>

export module cm.core:channel;
import std;
import :task;

namespace cm {
namespace asio = boost::asio;

/// A buffered asio channel that carries values of `T`. It is not thread-safe and must only be used on the io_context's thread.
export template <typename T>
class Channel : public asio::experimental::channel<void(boost::system::error_code, T)>
{
  public:
    using asio::experimental::channel<void(boost::system::error_code, T)>::channel;
};

/**
 * Receives the next value from `channel`.
 *
 * A stop request completes the task with `set_stopped`.
 *
 * @throws boost::system::system_error If the channel was closed.
 */
export template <typename T>
Task<T> async_receive(Channel<T>& channel)
{
    auto [ec, value] = co_await channel.async_receive(asio::as_tuple(exec::asio::use_sender));
    if (ec == asio::experimental::error::channel_cancelled || ec == asio::error::operation_aborted) {
        co_yield stdexec::with_stopped{};
    }
    if (ec) {
        throw boost::system::system_error{ec};
    }
    co_return std::move(value);
}

/**
 * Sends `value` into `channel`, waiting while the channel's buffer is full.
 *
 * A stop request completes the task with `set_stopped`.
 *
 * @throws boost::system::system_error If the channel was closed.
 */
export template <typename T>
Task<void> async_send(Channel<T>& channel, T value)
{
    auto [ec] =
        co_await channel.async_send(boost::system::error_code{}, std::move(value), asio::as_tuple(exec::asio::use_sender));
    if (ec == asio::experimental::error::channel_cancelled || ec == asio::error::operation_aborted) {
        co_yield stdexec::with_stopped{};
    }
    if (ec) {
        throw boost::system::system_error{ec};
    }
}
} // namespace cm
