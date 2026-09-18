module;
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <stdexec/execution.hpp>
export module cm.core:awaitable_bool;
import std;

namespace asio = boost::asio;

namespace cm {
/**
 * A boolean flag that can be waited on until it becomes `true`.
 *
 * It is not thread-safe. Setting the flag, waiting on it and requesting a waiter to stop must all happen on the thread
 * that runs `exec`.
 */
export class AwaitableBool
{
    struct Waiter
    {
        void (*complete_value)(Waiter*) noexcept;
        bool woken = false;
    };

  public:
    explicit AwaitableBool(asio::any_io_executor exec)
        : exec_(std::move(exec))
    {
    }

    AwaitableBool(const AwaitableBool&) = delete ("Prevent dangling refs in waiting operations");
    AwaitableBool& operator=(const AwaitableBool&) = delete;

    AwaitableBool& operator=(bool v)
    {
        if (v && !value_) {
            // false → true: wake all
            value_ = true;
            auto ws = std::exchange(waiters_, {});
            for (auto* w : ws) {
                w->woken = true;
                asio::post(exec_, [w]() noexcept { w->complete_value(w); });
            }
        }
        else {
            // true → false: reset, new waiters will come.
            value_ = v;
        }
        return *this;
    }

    explicit operator bool() const noexcept
    {
        return value_;
    }

  private:
    template <typename Receiver>
    struct Operation : Waiter
    {
        using operation_state_concept = stdexec::operation_state_tag;

        struct OnStop
        {
            Operation* self;

            void operator()() const noexcept
            {
                self->complete_stopped();
            }
        };

        using StopToken = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
        using StopCallback = stdexec::stop_callback_for_t<StopToken, OnStop>;

        AwaitableBool* flag;
        Receiver receiver;
        std::optional<StopCallback> on_stop;

        Operation(AwaitableBool* flag_, Receiver receiver_) noexcept
            : Waiter{.complete_value = &Operation::wake}
            , flag{flag_}
            , receiver{std::move(receiver_)}
        {
        }

        Operation(Operation&&) = delete;

        void start() & noexcept
        {
            if (flag->value_) {
                stdexec::set_value(std::move(receiver));
                return;
            }
            const auto token = stdexec::get_stop_token(stdexec::get_env(receiver));
            if (token.stop_requested()) {
                stdexec::set_stopped(std::move(receiver));
                return;
            }
            flag->waiters_.push_back(this);
            on_stop.emplace(token, OnStop{this});
        }

        static void wake(Waiter* waiter) noexcept
        {
            auto* self = static_cast<Operation*>(waiter);
            self->on_stop.reset();
            stdexec::set_value(std::move(self->receiver));
        }

        void complete_stopped() noexcept
        {
            // A woken waiter is completed by the posted value completion.
            if (woken) {
                return;
            }
            std::erase(flag->waiters_, static_cast<Waiter*>(this));
            on_stop.reset();
            stdexec::set_stopped(std::move(receiver));
        }
    };

    struct Sender
    {
        using sender_concept = stdexec::sender_tag;

        AwaitableBool* flag;

        template <typename Self, typename... Env>
        static consteval auto get_completion_signatures() noexcept
        {
            return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_stopped_t()>{};
        }

        template <stdexec::receiver Receiver>
        [[nodiscard]] Operation<Receiver> connect(Receiver receiver) const noexcept
        {
            return Operation<Receiver>{flag, std::move(receiver)};
        }
    };

  public:
    /// Completes once the flag is `true` (right away if it already is), or with `set_stopped` on a stop request.
    [[nodiscard]] Sender async_wait() noexcept
    {
        return Sender{this};
    }

  private:
    bool value_ = false;
    asio::any_io_executor exec_;
    std::vector<Waiter*> waiters_;
};

} // namespace cm
