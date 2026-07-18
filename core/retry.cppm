module;

export module cm.core:retry;
import std;

using namespace std::chrono_literals;

namespace cm {
export class TimeoutError : public std::runtime_error
{
  public:
    using runtime_error::runtime_error;
};

using namespace std::chrono_literals;

export struct ExponentialBackoffPolicy
{
    using milliseconds = std::chrono::milliseconds;

    milliseconds initial{100ms};
    milliseconds maximum{30s};

    milliseconds operator()(std::size_t retry) const
    {
        milliseconds delay = initial;

        for (std::size_t i = 0; i < retry && delay < maximum; ++i) {
            delay = std::min(delay * 2, maximum);
        }

        return delay;
    }
};

export template <typename F, typename Policy = ExponentialBackoffPolicy>
auto retry_on_timeout(std::size_t max_retries, F f, Policy policy = {}) -> decltype(f(std::chrono::milliseconds{}))
{
    for (std::size_t retry = 0;; ++retry) {
        try {
            co_return co_await f(policy(retry));
        }
        catch (const TimeoutError&) {
            if (retry >= max_retries) {
                throw;
            }
        }
    }
}
} // namespace cm
