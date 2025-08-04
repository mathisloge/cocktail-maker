#pragma once
#include <memory>
#include <boost/asio/awaitable.hpp>

namespace cm
{
class ExecutionContext;
class Recipe;
class RecipeExecutor
{
  public:
    explicit RecipeExecutor(std::shared_ptr<ExecutionContext> ctx, std::shared_ptr<Recipe> recipe);
    ~RecipeExecutor();

    void run();
    void cancel();

  private:
    std::shared_ptr<ExecutionContext> ctx_;
    std::shared_ptr<Recipe> recipe_;
    boost::asio::cancellation_signal cancel_signal_;
};
} // namespace cm
