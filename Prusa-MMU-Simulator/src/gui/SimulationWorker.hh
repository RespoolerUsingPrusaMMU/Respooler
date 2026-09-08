#ifndef PRUSA_MMU_SIMULATOR_SIMULATIONWORKER_HH
#define PRUSA_MMU_SIMULATOR_SIMULATIONWORKER_HH

#include "ControlInterface.hh"
#include "MmuBoard.hh"
#include "SimulatorController.hh"
#include "SimulatorStatus.hh"

#include <QElapsedTimer>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <string>

Q_DECLARE_METATYPE(SimulatorStatus)

class SimulationWorker : public QObject
{
  Q_OBJECT

public:
  SimulationWorker(const std::string &aFirmwarePath,
                   const std::string &aControlPath,
                   std::uint32_t aFrequency,
                   int aGdbPort,
                   bool aWaitForGdb);

public slots:
  void initialize();
  void runSlice();
  void executeCommand(const QString &aCommand);
  void tapButton(int aButton, int aMilliseconds);
  void holdButton(int aButton);
  void releaseButton();
  void stop();

signals:
  void initialized(bool aSuccess, const QString &aMessage);
  void statusChanged(const SimulatorStatus &aStatus);
  void logMessage(const QString &aMessage);
  void simulationFinished(int aExitCode);

private:
  void updateTap();
  std::uint64_t millisecondsToCycles(std::uint32_t aMilliseconds) const;

  std::string mFirmwarePath;
  std::string mControlPath;
  std::uint32_t mFrequency;
  int mGdbPort;
  bool mWaitForGdb;
  MmuBoard mBoard;
  ControlInterface mControl;
  SimulatorController mController;
  QTimer *mRunTimer;
  QElapsedTimer mStatusTimer;
  bool mRunning;
  MmuBoard::Button mTapButton;
  int mTapPhase;
  std::uint64_t mTapPhaseEndCycle;
  std::uint32_t mTapHoldMilliseconds;
};

#endif
