#pragma once
namespace cm
{
class DispenseLiquidCmd;
class ManualCmd;
class CommandVisitor
{
  public:
    virtual void visit(const DispenseLiquidCmd &cmd) = 0;
    virtual void visit(const ManualCmd &cmd) = 0;
    virtual ~CommandVisitor() = default;
};
} // namespace cm
