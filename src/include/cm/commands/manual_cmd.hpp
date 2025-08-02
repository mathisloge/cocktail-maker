#pragma once
#include "command.hpp"
namespace cm
{
class ManualCmd : public Command
{
  public:
    ManualCmd(std::string instruction);
    boost::asio::awaitable<void> run(ExecutionContext &ctx) override;

  private:
    std::string instruction_;
};
} // namespace cm
