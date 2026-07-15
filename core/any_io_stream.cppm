module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/system/error_code.hpp>

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
    SocketIoStream(TSocket socket);

    boost::cobalt::task<std::tuple<boost::system::error_code, int>> async_read(asio::mutable_buffer buffer) override;
    boost::cobalt::task<void> async_write(asio::const_buffer buffer, std::chrono::milliseconds timeout) override;
    boost::system::error_code close() override;
    boost::asio::any_io_executor get_executor() override;
};

} // namespace cm
