// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <memory>
#include <spdlog/fwd.h>
#include <spdlog/logger.h>
#include <vector>

namespace cm {
class LoggingContext
{
  public:
    static LoggingContext& instance();
    std::shared_ptr<spdlog::logger> create_logger(std::string name);
    ~LoggingContext();

  private:
    LoggingContext();

  private:
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks_;
};
} // namespace cm
