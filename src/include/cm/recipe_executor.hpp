// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <boost/asio/awaitable.hpp>
#include <memory>

namespace cm {
class ExecutionContext;
class Recipe;

class RecipeExecutor
{
  public:
    explicit RecipeExecutor(std::shared_ptr<ExecutionContext> ctx, std::shared_ptr<Recipe> recipe);
    ~RecipeExecutor();

    void continue_execution();
    void run();

  private:
    std::shared_ptr<ExecutionContext> ctx_;
    std::shared_ptr<Recipe> recipe_;
    boost::asio::cancellation_signal cancel_signal_;
};
} // namespace cm
