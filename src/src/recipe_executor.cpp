#include "cm/recipe_executor.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <spdlog/spdlog.h>
#include "cm/commands/command.hpp"
#include "cm/execution_context.hpp"
#include "cm/logging.hpp"
#include "cm/recipe.hpp"

namespace cm
{
RecipeExecutor::RecipeExecutor(std::shared_ptr<ExecutionContext> ctx, std::shared_ptr<Recipe> recipe)
    : ctx_{std::move(ctx)}
    , recipe_{std::move(recipe)}
{}

RecipeExecutor::~RecipeExecutor()
{
    cancel_signal_.emit(boost::asio::cancellation_type::all);
};

void RecipeExecutor::run()
{
    boost::asio::co_spawn(
        ctx_->io_context(),
        [ctx = ctx_, recipe = recipe_] -> boost::asio::awaitable<void> {
            auto logger = LoggingContext::instance().create_logger("RecipeExecutor");
            SPDLOG_LOGGER_DEBUG(logger, "Starting producing {}", *recipe);
            for (auto &&step : recipe->production_steps())
            {
                for (auto &&cmd : step)
                {
                    co_await cmd->run(*ctx);
                }
            }
        },
        boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::detached));
}
} // namespace cm
