#include "cm/machine_state.hpp"
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include "cm/hw/hx711_sensor.hpp"

namespace cm
{

MachineState::MachineState(boost::asio::io_context &io)
    : load_cell_{std::make_unique<Hx711Sensor>(cm::Hx711DatPin{.chip = "/dev/gpiochip0", .offset = {24}},
                                               cm::Hx711ClkPin{.chip = "/dev/gpiochip0", .offset = {23}})}
{
    boost::asio::co_spawn(
        io,
        [this, load_cell = load_cell_] -> boost::asio::awaitable<void> {
            boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};

            fmt::println("start");
            timer.expires_after(std::chrono::seconds{1});
            co_await timer.async_wait(boost::asio::use_awaitable);
            co_await load_cell->tare();

            fmt::println("place ref weight");
            timer.expires_after(std::chrono::seconds{5});
            co_await timer.async_wait(boost::asio::use_awaitable);
            co_await load_cell->calibrate_with_ref_weight(100 * mp_units::si::gram);

            while (true)
            {
                measured_weight_ = co_await load_cell->read();
                fmt::println("read: {}", measured_weight_);
                co_await timer.async_wait(boost::asio::use_awaitable);
            }
            co_return;
        },
        boost::asio::bind_cancellation_slot(cancel_signal_.slot(), boost::asio::detached));
}

MachineState::~MachineState()
{
    cancel_signal_.emit(boost::asio::cancellation_type::all);
}

units::OperationalState MachineState::load_cell_status() const
{
    return units::OperationalState::ok;
}

units::Grams MachineState::last_measured_weight() const
{
    return measured_weight_;
}

} // namespace cm
