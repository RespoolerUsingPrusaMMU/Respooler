#ifndef PRUSA_MMU_SIMULATOR_SIMULATORCONTROLLER_HH
#define PRUSA_MMU_SIMULATOR_SIMULATORCONTROLLER_HH

#include "MmuBoard.hh"
#include "SimulatorStatus.hh"

#include <cstdint>
#include <string>

class SimulatorController
{
public:
  explicit SimulatorController(MmuBoard &aBoard);

  void setFilamentPresent(bool aPresent);
  void setButton(MmuBoard::Button aButton);
  void setStall(MmuBoard::Axis aAxis, bool aStalled);
  void setDriverPresent(MmuBoard::Axis aAxis, bool aPresent);
  void setDriverIdentityValid(MmuBoard::Axis aAxis, bool aValid);
  void setDriverUnderVoltage(MmuBoard::Axis aAxis, bool aOn);
  void setDriverOverTemperature(MmuBoard::Axis aAxis, bool aOn);
  void setDriverPrewarn(MmuBoard::Axis aAxis, bool aOn);
  void setAutomaticMechanics(bool aOn);
  void setShuttlePosition(std::int64_t aPosition);
  void setRunoutAfterTakeupSteps(std::uint64_t aAdditionalSteps);
  void disableAutomaticRunout();

  SimulatorStatus status() const;

private:
  MmuBoard &mBoard;
};

#endif
