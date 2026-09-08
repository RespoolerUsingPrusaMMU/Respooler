#include "SimulatorController.hh"

SimulatorController::SimulatorController(MmuBoard &aBoard)
  : mBoard(aBoard)
{
}

void SimulatorController::setFilamentPresent(bool aPresent)
{
  mBoard.setFilamentPresent(aPresent);
}

void SimulatorController::setButton(MmuBoard::Button aButton)
{
  mBoard.setButton(aButton);
}

void SimulatorController::setStall(MmuBoard::Axis aAxis, bool aStalled)
{
  mBoard.setStall(aAxis, aStalled);
}

void SimulatorController::setDriverPresent(MmuBoard::Axis aAxis, bool aPresent)
{
  mBoard.setDriverPresent(aAxis, aPresent);
}

void SimulatorController::setDriverIdentityValid(MmuBoard::Axis aAxis, bool aValid)
{
  mBoard.setDriverIdentityValid(aAxis, aValid);
}

void SimulatorController::setDriverUnderVoltage(MmuBoard::Axis aAxis, bool aOn)
{
  mBoard.setDriverUnderVoltage(aAxis, aOn);
}

void SimulatorController::setDriverOverTemperature(MmuBoard::Axis aAxis, bool aOn)
{
  mBoard.setDriverOverTemperature(aAxis, aOn);
}

void SimulatorController::setDriverPrewarn(MmuBoard::Axis aAxis, bool aOn)
{
  mBoard.setDriverPrewarn(aAxis, aOn);
}

void SimulatorController::setAutomaticMechanics(bool aOn)
{
  mBoard.setAutomaticMechanics(aOn);
}

void SimulatorController::setShuttlePosition(std::int64_t aPosition)
{
  mBoard.setShuttlePosition(aPosition);
}

void SimulatorController::setRunoutAfterTakeupSteps(std::uint64_t aAdditionalSteps)
{
  mBoard.setRunoutAfterTakeupSteps(aAdditionalSteps);
}

void SimulatorController::disableAutomaticRunout()
{
  mBoard.disableAutomaticRunout();
}

SimulatorStatus SimulatorController::status() const
{
  return mBoard.statusSnapshot();
}
