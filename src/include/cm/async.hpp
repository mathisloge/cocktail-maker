// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>

namespace cm {
template <typename T>
using async = boost::asio::awaitable<T>;
}
