module;
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <stdexec/execution.hpp>

export module cm.core:io_scheduler;
import std;

namespace cm {
namespace asio = boost::asio;

/**
 * A stdexec scheduler that runs work on an asio io_context.
 *
 * `schedule()` posts to the io_context, so it can be used from any thread to hop onto the Station's execution context.
 * The schedule operation only completes with `set_value`. It neither observes stop requests nor reports errors, because
 * `stdexec::task` requires an infallible scheduler for resuming on its scheduler after every `co_await`. A failing `post`
 * (out of memory) terminates.
 */
export class IoScheduler
{
  public:
    using scheduler_concept = stdexec::scheduler_tag;
    using executor_type = asio::io_context::executor_type;

    explicit IoScheduler(executor_type executor) noexcept
        : executor_{std::move(executor)}
    {
    }

  private:
    template <typename Receiver>
    struct Operation
    {
        using operation_state_concept = stdexec::operation_state_tag;

        executor_type executor;
        Receiver receiver;

        void start() & noexcept
        {
            asio::post(executor, [this]() noexcept { stdexec::set_value(std::move(receiver)); });
        }
    };

    struct Env
    {
        executor_type executor;

        [[nodiscard]] IoScheduler query(stdexec::get_completion_scheduler_t<stdexec::set_value_t>) const noexcept
        {
            return IoScheduler{executor};
        }
    };

    struct Sender
    {
        using sender_concept = stdexec::sender_tag;

        executor_type executor;

        template <typename Self, typename... Env>
        static consteval auto get_completion_signatures() noexcept
        {
            return stdexec::completion_signatures<stdexec::set_value_t()>{};
        }

        template <stdexec::receiver Receiver>
        [[nodiscard]] Operation<Receiver> connect(Receiver receiver) const noexcept
        {
            return Operation<Receiver>{executor, std::move(receiver)};
        }

        [[nodiscard]] Env get_env() const noexcept
        {
            return Env{executor};
        }
    };

  public:
    [[nodiscard]] Sender schedule() const noexcept
    {
        return Sender{executor_};
    }

    [[nodiscard]] executor_type executor() const noexcept
    {
        return executor_;
    }

    bool operator==(const IoScheduler&) const noexcept = default;

  private:
    executor_type executor_;
};
} // namespace cm
