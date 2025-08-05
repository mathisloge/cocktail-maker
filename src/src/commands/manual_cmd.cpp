#include "cm/commands/manual_cmd.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/events/manual_action_event.hpp"
#include "cm/execution_context.hpp"

namespace cm
{
ManualCmd::ManualCmd(std::string instruction)
    : instruction_{std::move(instruction)}
{}

const std::string &ManualCmd::instruction() const
{
    return instruction_;
}

boost::asio::awaitable<void> ManualCmd::run(ExecutionContext &ctx) const
{
    ctx.event_bus().publish(ManualActionEvent{.instruction = instruction_});
    co_await ctx.wait_for_resume();
}

void ManualCmd::accept(CommandVisitor &visitor) const
{
    visitor.visit(*this);
}

} // namespace cm
