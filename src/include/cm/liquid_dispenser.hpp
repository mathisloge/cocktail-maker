#pragma once
#include <boost/asio/awaitable.hpp>
#include "cm/units.hpp"
#include <mp-units/systems/si.h>
namespace cm
{
class LiquidDispenser
{
  public:
    virtual ~LiquidDispenser() = default;
    virtual boost::asio::awaitable<void> dispense(units::Litre volume) = 0;
    virtual const std::string &name() const = 0;
    virtual units::Litre remaining_volume() const = 0;
};
} // namespace cm
