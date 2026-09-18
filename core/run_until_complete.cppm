module;
#include <boost/asio/io_context.hpp>
#include <stdexec/execution.hpp>

export module cm.core:run_until_complete;
import std;
import :io_scheduler;

namespace cm {
namespace asio = boost::asio;

namespace detail {
struct RunCompletion
{
    bool done = false;
    bool stopped = false;
    std::exception_ptr error;
};

struct RunReceiver
{
    using receiver_concept = stdexec::receiver_tag;

    RunCompletion* completion;

    template <typename... Values>
    void set_value(Values&&...) && noexcept
    {
        completion->done = true;
    }

    template <typename Error>
    void set_error(Error&& error) && noexcept
    {
        if constexpr (std::same_as<std::remove_cvref_t<Error>, std::exception_ptr>) {
            completion->error = std::forward<Error>(error);
        }
        else {
            completion->error = std::make_exception_ptr(std::forward<Error>(error));
        }
        completion->done = true;
    }

    void set_stopped() && noexcept
    {
        completion->stopped = true;
        completion->done = true;
    }
};
} // namespace detail

/**
 * Runs `sender` on `io_context` from the calling thread until it has completed.
 *
 * The calling thread drives the io_context, so this must not be called while another thread runs it. Values of the
 * sender are discarded. Capture them inside the sender if they are needed.
 *
 * @returns `true` if the sender completed with a value, `false` if it completed with `set_stopped`.
 * @throws The error the sender completed with, or `std::logic_error` if the io_context ran out of work first.
 */
export template <stdexec::sender Sender>
bool run_until_complete(asio::io_context& io_context, Sender&& sender)
{
    detail::RunCompletion completion;
    auto operation = stdexec::connect(stdexec::starts_on(IoScheduler{io_context.get_executor()}, std::forward<Sender>(sender)),
                                      detail::RunReceiver{&completion});
    stdexec::start(operation);

    io_context.restart();
    while (!completion.done) {
        if (io_context.run_one() == 0) {
            throw std::logic_error{"io_context ran out of work before the sender completed"};
        }
    }

    if (completion.error) {
        std::rethrow_exception(completion.error);
    }
    return !completion.stopped;
}
} // namespace cm
