#pragma once
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include "units.hpp"

namespace cm
{
class Hx711Sensor;

class MachineState
{
  public:
    explicit MachineState(boost::asio::io_context &io);
    ~MachineState();

    units::OperationalState load_cell_status() const;
    [[nodiscard]] units::Grams last_measured_weight() const;

  private:
    boost::asio::cancellation_signal cancel_signal_;
    std::shared_ptr<Hx711Sensor> load_cell_;
    units::Grams measured_weight_;
};
} // namespace cm
