// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    explicit MachineState(boost::asio::io_context &io, std::unique_ptr<Hx711Sensor> load_cell);
    ~MachineState();

    units::OperationalState load_cell_status() const;
    [[nodiscard]] units::Grams last_measured_weight() const;

  private:
    boost::asio::cancellation_signal cancel_signal_;
    std::unique_ptr<Hx711Sensor> load_cell_;
    units::Grams measured_weight_{};
};
} // namespace cm
