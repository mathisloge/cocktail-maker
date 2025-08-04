#pragma once
#include <memory>
#include <vector>
#include <spdlog/fwd.h>
#include <spdlog/logger.h>
namespace cm
{
class LoggingContext
{
  public:
    static LoggingContext &instance();
    std::shared_ptr<spdlog::logger> create_logger(std::string name);
    ~LoggingContext();

  private:
    LoggingContext();

  private:
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks_;
};
} // namespace cm
