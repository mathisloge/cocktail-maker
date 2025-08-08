// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include "command_id.hpp"
namespace cm
{
class ExecutionContext;
class CommandVisitor;
class Command
{
  public:
    explicit Command(CommandId command_id);
    virtual ~Command() = default;
    [[nodiscard]] CommandId id() const;
    [[nodiscard]] virtual boost::asio::awaitable<void> run(ExecutionContext &ctx) const = 0;
    virtual void accept(CommandVisitor &visitor) const = 0;

  private:
    CommandId command_id_{};
};
} // namespace cm
