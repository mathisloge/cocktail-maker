#include "cm/ui/ExecutionContextAdapter.hpp"
#include <libassert/assert.hpp>

namespace cm::ui {

ExecutionContextAdapter::ExecutionContextAdapter(std::shared_ptr<ExecutionContext> ctx)
    : ctx_{std::move(ctx)}
{
}

ExecutionContext& ExecutionContextAdapter::context()
{
    ASSERT(ctx_ != nullptr);
    return *ctx_;
}
} // namespace cm::ui
