#pragma once
#include "liquid_dispenser.hpp"
#include "machine_state.hpp"

namespace cm
{
class ValveLiquidDispenser : public LiquidDispenser
{
  public:
    ValveLiquidDispenser(std::string identifier,
                         std::shared_ptr<const MachineState> machine_state,
                         units::GramPerLitre liquid_weight);
    boost::asio::awaitable<void> dispense(mp_units::quantity<mp_units::si::litre> volume) override;
    const std::string &name() const override;

  private:
    std::string identifier_;
    std::shared_ptr<const MachineState> machine_state_;
    units::GramPerLitre liquid_weight_;
};
} // namespace cm
