module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/write.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/task.hpp>

module cm.core:any_io_stream_impl;
import std;
import :any_io_stream;

namespace cm {

template <typename TSocket>
SocketIoStream<TSocket>::SocketIoStream(TSocket socket)
    : socket_{std::move(socket)}
{
}

template <typename TSocket>
boost::cobalt::task<std::tuple<boost::system::error_code, int>> SocketIoStream<TSocket>::async_read(
    boost::asio::mutable_buffer buffer)
{
    co_return co_await socket_.async_read_some(std::move(buffer), boost::asio::as_tuple(boost::cobalt::use_op));
}

template <typename TSocket>
boost::cobalt::task<void> SocketIoStream<TSocket>::async_write(boost::asio::const_buffer buffer,
                                                               std::chrono::milliseconds timeout)
{
    co_await boost::asio::async_write(
        socket_, std::move(buffer), boost::asio::cancel_after(timeout, boost::asio::as_tuple(boost::asio::deferred)));
}

template <typename TSocket>
boost::system::error_code SocketIoStream<TSocket>::close()
{
    boost::system::error_code ec;
    socket_.close(ec);
    return ec;
}

template <typename TSocket>
boost::asio::any_io_executor SocketIoStream<TSocket>::get_executor()
{
    return socket_.get_executor();
}

// Explicit instantiations for whatever socket types your core library actually uses
template class SocketIoStream<boost::asio::serial_port>;
template class SocketIoStream<boost::asio::local::stream_protocol::socket>;

} // namespace cm
