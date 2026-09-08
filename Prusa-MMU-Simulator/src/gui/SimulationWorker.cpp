#include "SimulationWorker.hh"

#include <QString>

#include <iostream>

namespace
{
static const std::uint32_t BUTTON_RELEASE_PHASE_MS = 50U;
static const int AVR_RUN_CALLS_PER_SLICE = 2000;
}

SimulationWorker::SimulationWorker(const std::string &aFirmwarePath,
                                   const std::string &aControlPath,
                                   std::uint32_t aFrequency,
                                   int aGdbPort,
                                   bool aWaitForGdb)
  : mFirmwarePath(aFirmwarePath),
    mControlPath(aControlPath),
    mFrequency(aFrequency),
    mGdbPort(aGdbPort),
    mWaitForGdb(aWaitForGdb),
    mControl(aControlPath),
    mController(mBoard),
    mRunTimer(nullptr),
    mRunning(false),
    mTapButton(MmuBoard::Button::None),
    mTapPhase(0),
    mTapPhaseEndCycle(0ULL),
    mTapHoldMilliseconds(500U)
{
}

void SimulationWorker::initialize()
{
  if(!mBoard.loadFirmware(mFirmwarePath))
  {
    emit initialized(false, "Unable to load firmware ELF.");
    return;
  }

  if(!mBoard.initialize(mFrequency, mGdbPort, mWaitForGdb))
  {
    emit initialized(false, "Unable to initialize simulated ATmega32U4.");
    return;
  }

  if(!mControl.initialize())
  {
    emit initialized(false, "Unable to create simulator FIFO.");
    return;
  }

  mRunTimer = new QTimer(this);
  mRunTimer->setTimerType(Qt::PreciseTimer);
  mRunTimer->setInterval(1);
  connect(mRunTimer, &QTimer::timeout, this, &SimulationWorker::runSlice);

  mRunning = true;
  mStatusTimer.start();
  mRunTimer->start();

  std::cout << "Simulator ready. GDB localhost:" << mGdbPort
            << ", FIFO " << mControlPath
            << (mWaitForGdb ? ", waiting for GDB\n" : ", firmware running\n");

  const QString tRunState = mWaitForGdb
    ? QString("Waiting for GDB attach")
    : QString("Firmware running");

  emit initialized(true,
                   QString("%1. GDB localhost:%2, FIFO %3")
                     .arg(tRunState)
                     .arg(mGdbPort)
                     .arg(QString::fromStdString(mControlPath)));
}

void SimulationWorker::runSlice()
{
  if(!mRunning)
    return;

  int tState = cpu_Stopped;
  for(int tRunIndex = 0; tRunIndex < AVR_RUN_CALLS_PER_SLICE; ++tRunIndex)
  {
    tState = avr_run(mBoard.avr());

    if(tState == cpu_Crashed)
      break;
  }

  // Polling the FIFO here, rather than only after a long blocking run loop,
  // keeps scripted controls responsive even while GDB has the AVR stopped.
  mControl.poll(mBoard);
  updateTap();

  // Limit GUI refresh traffic to approximately 30 frames/second. The AVR
  // itself continues to run as fast as simavr allows between refreshes.
  if(!mStatusTimer.isValid() || mStatusTimer.elapsed() >= 33)
  {
    emit statusChanged(mController.status());
    mStatusTimer.restart();
  }

  //--------------------------------------------------------------------
  // A crashed AVR is always fatal.  cpu_Done, however, can be returned
  // transiently by simavr while its GDB server is changing execution
  // state.  Do not terminate the Qt simulator simply because GDB caused
  // simavr to report cpu_Done.
  //--------------------------------------------------------------------
  if(tState == cpu_Crashed)
  {
    mRunning = false;
    mRunTimer->stop();
    emit simulationFinished(4);
  }
}

void SimulationWorker::executeCommand(const QString &aCommand)
{
  if(!mRunning || mBoard.avr() == nullptr)
  {
    emit logMessage(QString("Ignored command while simulator is not initialized: %1").arg(aCommand));
    return;
  }

  mControl.executeCommand(mBoard, aCommand.toStdString());
  emit logMessage(QString("GUI command: %1").arg(aCommand));
}

void SimulationWorker::tapButton(int aButton, int aMilliseconds)
{
  if(!mRunning || mBoard.avr() == nullptr)
  {
    emit logMessage("Ignored button tap because the simulator is not initialized.");
    return;
  }

  const MmuBoard::Button tButton = static_cast<MmuBoard::Button>(aButton);
  if(tButton == MmuBoard::Button::None)
    return;

  mBoard.setButton(MmuBoard::Button::None);
  mTapButton = tButton;
  mTapHoldMilliseconds = static_cast<std::uint32_t>(aMilliseconds < 40 ? 40 : aMilliseconds);
  mTapPhase = 1;
  mTapPhaseEndCycle = mBoard.avr()->cycle + millisecondsToCycles(BUTTON_RELEASE_PHASE_MS);
}

void SimulationWorker::holdButton(int aButton)
{
  if(!mRunning || mBoard.avr() == nullptr)
  {
    emit logMessage("Ignored button hold because the simulator is not initialized.");
    return;
  }

  mTapPhase = 0;
  mTapButton = MmuBoard::Button::None;
  mBoard.setButton(static_cast<MmuBoard::Button>(aButton));
}

void SimulationWorker::releaseButton()
{
  if(mBoard.avr() == nullptr)
    return;

  mTapPhase = 0;
  mTapButton = MmuBoard::Button::None;
  mBoard.setButton(MmuBoard::Button::None);
}

void SimulationWorker::stop()
{
  mRunning = false;
  if(mRunTimer != nullptr)
    mRunTimer->stop();
}

void SimulationWorker::updateTap()
{
  if(mTapPhase == 0 || mBoard.avr() == nullptr)
    return;

  if(mBoard.avr()->cycle < mTapPhaseEndCycle)
    return;

  if(mTapPhase == 1)
  {
    mBoard.setButton(mTapButton);
    mTapPhase = 2;
    mTapPhaseEndCycle = mBoard.avr()->cycle + millisecondsToCycles(mTapHoldMilliseconds);
  }
  else if(mTapPhase == 2)
  {
    mBoard.setButton(MmuBoard::Button::None);
    mTapPhase = 3;
    mTapPhaseEndCycle = mBoard.avr()->cycle + millisecondsToCycles(BUTTON_RELEASE_PHASE_MS);
  }
  else
  {
    mTapPhase = 0;
    mTapButton = MmuBoard::Button::None;
    mTapPhaseEndCycle = 0ULL;
  }
}

std::uint64_t SimulationWorker::millisecondsToCycles(std::uint32_t aMilliseconds) const
{
  if(mBoard.avr() == nullptr || mBoard.avr()->frequency == 0U)
    return 0ULL;

  return (static_cast<std::uint64_t>(aMilliseconds) *
          static_cast<std::uint64_t>(mBoard.avr()->frequency)) / 1000ULL;
}
