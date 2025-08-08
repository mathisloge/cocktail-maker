// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "command.hpp"

namespace cm {
class ManualCmd : public Command
{
  public:
    explicit ManualCmd(std::string instruction, CommandId id);
    const std::string& instruction() const;
    boost::asio::awaitable<void> run(ExecutionContext& ctx) const override;
    void accept(CommandVisitor& visitor) const override;

  private:
    std::string instruction_;
};
} // namespace cm
