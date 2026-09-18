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

export class AnyIoStream
{
  public:
    virtual ~AnyIoStream() = default;
    [[nodiscard]] virtual auto async_read(asio::mutable_buffer buffer)
        -> Task<std::tuple<boost::system::error_code, std::size_t>> = 0;
    [[nodiscard]] virtual auto async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout) -> Task<void> = 0;
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
        co_return co_await socket_.async_read_some(std::move(buffer), asio::as_tuple(exec::asio::use_sender));
    }

    Task<void> async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout) override
    {
        co_await asio::async_write(
            socket_, std::move(buffer), asio::cancel_after(timeout, asio::as_tuple(exec::asio::use_sender)));
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
