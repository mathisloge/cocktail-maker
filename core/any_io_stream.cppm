module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/write.hpp>
#include <exec/asio/use_sender.hpp>
#include <stdexec/execution.hpp>

export module cm.core:any_io_stream;
import std;
import :task;

namespace cm {
namespace asio = boost::asio;

/**
 * A byte stream to a Pod.
 *
 * A stop request completes a pending read or write with `set_stopped`, and every other outcome comes back as an
 * `error_code`.
 */
export class AnyIoStream
{
  public:
    virtual ~AnyIoStream() = default;
    [[nodiscard]] virtual auto async_read(asio::mutable_buffer buffer)
        -> Task<std::tuple<boost::system::error_code, std::size_t>> = 0;
    /// @returns An empty code on success, `asio::error::timed_out` once `timeout` has passed, or the error of the write.
    [[nodiscard]] virtual auto async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout)
        -> Task<boost::system::error_code> = 0;
    [[nodiscard]] virtual auto close() -> boost::system::error_code = 0;
    [[nodiscard]] virtual auto get_executor() -> asio::any_io_executor = 0;
};

export template <typename TSocket>
class SocketIoStream : public AnyIoStream
{
    TSocket socket_;

  public:
    SocketIoStream(TSocket socket)
        : socket_{std::move(socket)}
    {
    }

    Task<std::tuple<boost::system::error_code, std::size_t>> async_read(asio::mutable_buffer buffer) override
    {
        auto [ec, bytes_read] = co_await socket_.async_read_some(std::move(buffer), asio::as_tuple(exec::asio::use_sender));
        if (ec == asio::error::operation_aborted && (co_await stdexec::read_env(stdexec::get_stop_token)).stop_requested()) {
            co_yield stdexec::with_stopped{};
        }
        co_return std::tuple{ec, bytes_read};
    }

    Task<boost::system::error_code> async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout) override
    {
        auto [ec, bytes_written] = co_await asio::async_write(
            socket_, std::move(buffer), asio::cancel_after(timeout, asio::as_tuple(exec::asio::use_sender)));
        if (ec == asio::error::operation_aborted) {
            if ((co_await stdexec::read_env(stdexec::get_stop_token)).stop_requested()) {
                co_yield stdexec::with_stopped{};
            }
            // Nothing but the timeout cancels the write otherwise.
            co_return asio::error::timed_out;
        }
        co_return ec;
    }

    boost::system::error_code close() override
    {
        boost::system::error_code ec;
        socket_.close(ec);
        return ec;
    }

    boost::asio::any_io_executor get_executor() override
    {
        return socket_.get_executor();
    }
};

} // namespace cm
