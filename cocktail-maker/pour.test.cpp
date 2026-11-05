#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <exec/asio/use_sender.hpp>
#include <exec/when_any.hpp>
#include <proto/field/ErrorCodeCommon.h>
#include <stdexec/execution.hpp>

import std;
import mp_units;
import cm.core;
import cm;

#include "fake_dispenser.test.hpp"

using namespace std::chrono_literals;
namespace units = cm::units;
using enum cm::DispenseOutcome;

namespace {
const cm::PodDispenser kAddress{cm::PodId{"Pod"}, cm::DispenserId{0}};

cm::DispenseReport report(double amount, cm::DispenseOutcome outcome)
{
    return cm::DispenseReport{.amount = amount * units::milli_litre, .outcome = outcome};
}

struct PourFixture
{
    boost::asio::io_context ctx;
    std::filesystem::path file = std::filesystem::temp_directory_path() /
                                 std::format("pour-{}.json", std::chrono::steady_clock::now().time_since_epoch().count());
    cm::FillLevelTracker tracker{ctx.get_executor(), file, cm::FillLevelOptions{.refill_settle_time = 1ms}};
    FakeDispenser dispenser;
    int prompts = 0;
    double refill_amount = 0;

    ~PourFixture()
    {
        tracker.flush();
        std::filesystem::remove(file);
    }

    cm::RefillPrompt prompt()
    {
        return [this]() -> cm::Task<void> {
            ++prompts;
            dispenser.reading += refill_amount * units::si::gram;
            co_return;
        };
    }

    void pour(double volume)
    {
        REQUIRE(cm::run_until_complete(
            ctx, cm::pour_with_refill(dispenser, kAddress, volume * units::milli_litre, tracker, prompt())));
    }

    std::vector<long> requests() const
    {
        std::vector<long> result;
        for (const auto volume : dispenser.requested) {
            result.push_back(std::lround(volume.numerical_value_in(units::milli_litre)));
        }
        return result;
    }
};
} // namespace

TEST_CASE_METHOD(PourFixture, "A complete pour needs no prompt", "[pour]")
{
    dispenser.steps.emplace_back(report(40, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40});
    CHECK(prompts == 0);
}

TEST_CASE_METHOD(PourFixture, "An empty bottle prompts for a refill and pours the rest", "[pour]")
{
    tracker.empty_bottle_tared(kAddress);
    dispenser.steps.emplace_back(report(10, bottle_empty));
    dispenser.steps.emplace_back(report(30, completed));
    refill_amount = 700;

    pour(40);

    CHECK(requests() == std::vector<long>{40, 30});
    CHECK(prompts == 1);
    const auto record = tracker.record(kAddress);
    REQUIRE(record.has_value());
    CHECK(record->capacity == 700 * units::si::gram);
    CHECK_FALSE(record->suspected_empty);
}

TEST_CASE_METHOD(PourFixture, "A bottle still empty after a refill prompts again", "[pour]")
{
    dispenser.steps.emplace_back(report(10, bottle_empty));
    dispenser.steps.emplace_back(report(0, bottle_empty));
    dispenser.steps.emplace_back(report(30, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40, 30, 30});
    CHECK(prompts == 2);
}

TEST_CASE_METHOD(PourFixture, "The Pod's time limit continues without a prompt", "[pour]")
{
    dispenser.steps.emplace_back(report(120, timed_out));
    dispenser.steps.emplace_back(report(50, completed));

    pour(170);

    CHECK(requests() == std::vector<long>{170, 50});
    CHECK(prompts == 0);
}

TEST_CASE_METHOD(PourFixture, "A pour still short after its trims continues without a prompt", "[pour]")
{
    dispenser.steps.emplace_back(report(37, still_short));
    dispenser.steps.emplace_back(report(3, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40, 3});
    CHECK(prompts == 0);
}

TEST_CASE_METHOD(PourFixture, "A pour still short that barely moved prompts", "[pour]")
{
    tracker.empty_bottle_tared(kAddress);
    dispenser.steps.emplace_back(report(5, still_short));
    dispenser.steps.emplace_back(report(35, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40, 35});
    CHECK(prompts == 1);
}

TEST_CASE_METHOD(PourFixture, "A pour that barely moved in its time limit prompts", "[pour]")
{
    tracker.empty_bottle_tared(kAddress);
    dispenser.steps.emplace_back(report(15, timed_out));
    dispenser.steps.emplace_back(report(25, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40, 25});
    CHECK(prompts == 1);
}

TEST_CASE_METHOD(PourFixture, "Pouring too much asks for nothing", "[pour]")
{
    dispenser.steps.emplace_back(report(50, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40});
    CHECK(prompts == 0);
}

TEST_CASE_METHOD(PourFixture, "A DispenserEmptyError counts as an empty bottle with nothing poured", "[pour]")
{
    dispenser.steps.emplace_back(std::make_exception_ptr(cm::DispenserEmptyError{cm::PodId{"Fake"}, cm::DispenserId{0}}));
    dispenser.steps.emplace_back(report(40, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40, 40});
    CHECK(prompts == 1);
}

TEST_CASE_METHOD(PourFixture, "A failed weight read doesn't stop a pour", "[pour]")
{
    dispenser.no_load_cell = true;
    dispenser.steps.emplace_back(report(40, completed));

    pour(40);

    CHECK(requests() == std::vector<long>{40});
}

TEST_CASE_METHOD(PourFixture, "A pod that is gone reaches the caller", "[pour]")
{
    dispenser.steps.emplace_back(std::make_exception_ptr(std::runtime_error{"Pod lifetime exceeded"}));

    REQUIRE_THROWS_WITH(
        cm::run_until_complete(ctx, cm::pour_with_refill(dispenser, kAddress, 40 * units::milli_litre, tracker, prompt())),
        "Pod lifetime exceeded");
}

TEST_CASE_METHOD(PourFixture, "A stop during the prompt ends the pour", "[pour]")
{
    dispenser.steps.emplace_back(report(10, bottle_empty));

    cm::AwaitableBool never{ctx.get_executor()};
    cm::RefillPrompt waiting_prompt = [&never]() -> cm::Task<void> { co_await never.async_wait(); };
    const auto stop_soon = [this]() -> cm::Task<void> {
        boost::asio::steady_timer timer{ctx, 10ms};
        co_await timer.async_wait(exec::asio::use_sender);
    };

    const auto run = [&]() -> cm::Task<void> {
        co_await exec::when_any(cm::pour_with_refill(dispenser, kAddress, 40 * units::milli_litre, tracker, waiting_prompt),
                                stop_soon());
    };
    REQUIRE(cm::run_until_complete(ctx, run()));

    CHECK(requests() == std::vector<long>{40});
}
