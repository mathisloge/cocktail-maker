#pragma once
#include <string>

namespace cm
{
class ExecutionEventSink
{
  public:
    virtual ~ExecutionEventSink() = default;
    virtual void on_command_started(const std::string &name) = 0;
    virtual void on_command_finished(const std::string &name) = 0;
    virtual void on_manual_intervention_required(const std::string &message) = 0;
    virtual void on_error(const std::string &component, const std::string &message) = 0;
};

} // namespace cm
