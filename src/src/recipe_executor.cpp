// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cm/recipe_executor.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <spdlog/spdlog.h>
#include "cm/commands/command.hpp"
#include "cm/commands/command_visitor.hpp"
#include "cm/commands/dispense_liquid_cmd.hpp"
#include "cm/commands/manual_cmd.hpp"
#include "cm/execution_context.hpp"
#include "cm/logging.hpp"
#include "cm/recipe.hpp"

namespace cm {
namespace {
struct CommandDebugPrinter final : public CommandVisitor
{
    explicit CommandDebugPrinter(std::shared_ptr<spdlog::logger> logger)
        : logger{std::move(logger)}
    {
    }

    void visit(const DispenseLiquidCmd& cmd) override
    {
        SPDLOG_LOGGER_DEBUG(logger, "Begin DispenseLiquidCmd with {}={}", cmd.ingredient(), cmd.volume());
    }

    void visit(const ManualCmd& cmd) override
    {
        SPDLOG_LOGGER_DEBUG(logger, "Begin ManualCmd with {}", cmd.instruction());
    }

    std::shared_ptr<spdlog::logger> logger;
};
} // namespace

RecipeExecutor::RecipeExecutor(std::shared_ptr<ExecutionContext> ctx, std::shared_ptr<Recipe> recipe)
    : ctx_{std::move(ctx)}
    , recipe_{std::move(recipe)}
{
}

RecipeExecutor::~RecipeExecutor()
{
    cancel_signal_.emit(boost::asio::cancellation_type::all);
};

void RecipeExecutor::continue_execution()
{
    ctx_->resume();
}

void RecipeExecutor::run()
{
    boost::asio::co_spawn(
        ctx_->async_executor(),
        [ctx = ctx_, recipe = recipe_] -> boost::asio::awaitable<void> {
            auto logger = LoggingContext::instance().create_logger("RecipeExecutor");
            SPDLOG_LOGGER_DEBUG(logger, "Starting producing {}", *recipe);
            for (auto&& step : recipe->production_steps()) {
                for (auto&& cmd : step) {
                    CommandDebugPrinter debug_printer{logger};
                    cmd->accept(debug_printer);
                    try {
                        co_await cmd->run(*ctx);
                    }
                    catch (const std::exception& ex) {
                        SPDLOG_LOGGER_DEBUG(logger, "command failed with {}", ex.what());
                    }
                    SPDLOG_LOGGER_DEBUG(logger, "command finished");
                }
            }
            SPDLOG_LOGGER_DEBUG(logger, "Finished producing {}", *recipe);
            ctx->event_bus().publish(RecipeFinishedEvent{});
        },
        boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::detached));
}
} // namespace cm
