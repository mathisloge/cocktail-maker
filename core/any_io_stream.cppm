module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/task.hpp>

export module cm.core:any_io_stream;
import std;

namespace cm {
namespace asio = boost::asio;

export class AnyIoStream
{
  public:
    virtual ~AnyIoStream() = default;
    [[nodiscard]] virtual auto async_read(asio::mutable_buffer buffer)
        -> boost::cobalt::task<std::tuple<boost::system::error_code, int>> = 0;
    [[nodiscard]] virtual auto async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout)
        -> boost::cobalt::task<void> = 0;
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

    boost::cobalt::task<std::tuple<boost::system::error_code, int>> async_read(asio::mutable_buffer buffer) override
    {
        co_return co_await boost::asio::async_read(socket_, buffer, asio::as_tuple(boost::cobalt::use_op));
    }

    boost::cobalt::task<void> async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout) override
    {
        co_await asio::async_write(socket_, std::move(buffer), asio::cancel_after(timeout, asio::as_tuple(asio::deferred)));
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