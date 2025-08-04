#pragma once
#include <boost/asio/awaitable.hpp>
namespace cm
{
class ExecutionContext;
class Command
{
  public:
    virtual boost::asio::awaitable<void> run(ExecutionContext &ctx) const = 0;
    virtual ~Command() = default;
};
} // namespace cm
