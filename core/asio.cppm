module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/post.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/system/error_code.hpp>

export module cm.core:asio;

// Export only the TYPES, not the internal-linkage variables
export namespace boost {
namespace asio {
using asio::any_io_executor;
using asio::as_tuple;
using asio::async_write;
using asio::cancel_after;
using asio::const_buffer;
using asio::deferred_t; // Export type instead of 'deferred'
using asio::mutable_buffer;
using asio::post;
} // namespace asio

namespace cobalt {
using cobalt::promise;
using cobalt::task;
using cobalt::use_op_t; // Export type instead of 'use_op'
} // namespace cobalt

namespace system {
using system::error_code;
}
} // namespace boost

// Define our own inline constexpr instances in our project's namespace.
// These have external linkage and are completely safe to export and use.
export namespace cm::exec {
inline constexpr boost::asio::deferred_t deferred{};
inline constexpr boost::asio::detached_t detached{};
inline constexpr boost::cobalt::use_op_t use_op{};
} // namespace cm