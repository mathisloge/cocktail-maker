module;
#include <boost/hash2/hmac.hpp>
#include <boost/hash2/sha2.hpp>

export module cm.gui:operator_auth;

import std;

export namespace cm::gui {

struct OperatorAuthConfig
{
    std::string pin_hash; // encoded using hash_pin
    std::string salt;
    int max_attempts = 5;
    std::chrono::seconds lockout_duration{30};
};

std::string hash_pin(const std::string& pin, const std::string& salt)
{
    boost::hash2::hmac_sha2_256 hmac({reinterpret_cast<const unsigned char*>(salt.data()), salt.size()});
    hmac.update(pin.data(), pin.size());
    return boost::hash2::to_string(hmac.result());
}

class OperatorAuth
{
  public:
    explicit OperatorAuth(OperatorAuthConfig config)
        : config_{std::move(config)}
    {
    }

    bool verify(std::string const& pin, std::chrono::steady_clock::time_point now)
    {
        if (is_locked_out(now)) {
            return false;
        }
        if (hash_pin(pin, config_.salt) == config_.pin_hash) {
            failed_attempts_ = 0;
            locked_until_.reset();
            return true;
        }
        ++failed_attempts_;
        if (failed_attempts_ >= config_.max_attempts) {
            locked_until_ = now + config_.lockout_duration;
        }
        return false;
    }

    bool is_locked_out(std::chrono::steady_clock::time_point now) const
    {
        return locked_until_.has_value() && now < *locked_until_;
    }

    int remaining_attempts() const
    {
        return std::max(0, config_.max_attempts - failed_attempts_);
    }

    std::chrono::seconds lockout_remaining(std::chrono::steady_clock::time_point now) const
    {
        if (!is_locked_out(now)) {
            return std::chrono::seconds{0};
        }
        return std::chrono::duration_cast<std::chrono::seconds>(*locked_until_ - now);
    }

  private:
    OperatorAuthConfig config_;
    int failed_attempts_ = 0;
    std::optional<std::chrono::steady_clock::time_point> locked_until_;
};

} // namespace cm::gui
