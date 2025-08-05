#pragma once
#include <boost/asio/awaitable.hpp>
namespace cm
{
class ExecutionContext;
class CommandVisitor;
class Command
{
  public:
    virtual ~Command() = default;
    virtual boost::asio::awaitable<void> run(ExecutionContext &ctx) const = 0;
    virtual void accept(CommandVisitor &visitor) const = 0;
};
} // namespace cm
