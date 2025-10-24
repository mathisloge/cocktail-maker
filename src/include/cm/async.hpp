#pragma once
#include <boost/asio/awaitable.hpp>

namespace cm {
template <typename T>
using async = boost::asio::awaitable<T>;
}
