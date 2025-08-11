#pragma once
#include <boost/asio/awaitable.hpp>
#include "cm/units.hpp"

namespace cm {
class WeightSensor
{
  public:
    virtual ~WeightSensor() = default;
    virtual boost::asio::awaitable<void> tare() = 0;
    virtual boost::asio::awaitable<void> calibrate_with_ref_weight(units::Grams known_mass) = 0;
    [[nodiscard]] virtual boost::asio::awaitable<units::Grams> read() = 0;
};
} // namespace cm
