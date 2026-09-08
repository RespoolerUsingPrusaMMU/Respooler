#ifndef PRUSA_MMU_SIMULATOR_SIMULATORSTATUS_HH
#define PRUSA_MMU_SIMULATOR_SIMULATORSTATUS_HH

#include <array>
#include <cstdint>
#include <string>

struct MotorStatus
{
  std::string name;
  bool enabled;
  bool direction;
  bool stalled;
  std::int64_t position;
  std::uint64_t stepCount;
  bool driverPresent;
  bool driverIdentityValid;
  bool underVoltage;
  bool overTemperature;
  bool overTemperaturePrewarn;
};

struct SimulatorStatus
{
  bool initialized;
  bool filamentPresent;
  bool automaticMechanics;
  std::int64_t shuttleMinimum;
  std::int64_t shuttleMaximum;
  std::uint64_t cycle;
  std::array<MotorStatus, 3> motors;
  std::array<bool, 5> redLeds;
  std::array<bool, 5> greenLeds;
};

#endif
