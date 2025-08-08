// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace cm {
class DispenseLiquidCmd;
class ManualCmd;

class CommandVisitor
{
  public:
    virtual void visit(const DispenseLiquidCmd& cmd) = 0;
    virtual void visit(const ManualCmd& cmd) = 0;
    virtual ~CommandVisitor() = default;
};
} // namespace cm
