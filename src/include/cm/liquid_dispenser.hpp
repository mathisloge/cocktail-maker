#pragma once
#include <boost/asio/awaitable.hpp>
#include <mp-units/systems/si.h>
namespace cm
{
class LiquidDispenser
{
  public:
    virtual ~LiquidDispenser() = default;
    virtual boost::asio::awaitable<void> dispense(mp_units::quantity<mp_units::si::litre> volume) = 0;
    virtual const std::string &name() const = 0;
};
} // namespace cm
