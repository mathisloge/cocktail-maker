#include <catch2/catch_test_macros.hpp>
import cm.gui;

using namespace cm::gui;
using Clock = std::chrono::steady_clock;

TEST_CASE("OperatorAuth accepts correct pin", "[operator_auth]")
{
    auto salt = std::string{"test-salt"};
    OperatorAuth auth{OperatorAuthConfig{
        .pin_hash = hash_pin("1234", salt),
        .salt = salt,
    }};
    REQUIRE(auth.verify("1234", Clock::now()));
}

TEST_CASE("OperatorAuth rejects incorrect pin", "[operator_auth]")
{
    auto salt = std::string{"test-salt"};
    OperatorAuth auth{OperatorAuthConfig{
        .pin_hash = hash_pin("1234", salt),
        .salt = salt,
    }};
    REQUIRE_FALSE(auth.verify("0000", Clock::now()));
}

TEST_CASE("OperatorAuth locks out after max_attempts", "[operator_auth]")
{
    auto salt = std::string{"test-salt"};
    OperatorAuth auth{OperatorAuthConfig{
        .pin_hash = hash_pin("1234", salt),
        .salt = salt,
        .max_attempts = 3,
    }};
    auto now = Clock::now();
    for (int i = 0; i < 3; ++i) {
        REQUIRE_FALSE(auth.verify("wrong", now));
    }
    REQUIRE(auth.is_locked_out(now));
    REQUIRE_FALSE(auth.verify("1234", now)); // correct pin, but still locked
}

TEST_CASE("OperatorAuth unlocks after lockout_duration elapses", "[operator_auth]")
{
    auto salt = std::string{"test-salt"};
    OperatorAuth auth{OperatorAuthConfig{
        .pin_hash = hash_pin("1234", salt),
        .salt = salt,
        .max_attempts = 1,
        .lockout_duration = std::chrono::seconds{30},
    }};
    auto now = Clock::now();
    REQUIRE_FALSE(auth.verify("wrong", now));
    REQUIRE(auth.is_locked_out(now));

    auto later = now + std::chrono::seconds{31};
    REQUIRE(auth.verify("1234", later));
}