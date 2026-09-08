#ifndef PRUSA_MMU_SIMULATOR_MAINWINDOW_HH
#define PRUSA_MMU_SIMULATOR_MAINWINDOW_HH

#include "MachineWidget.hh"
#include "SimulatorStatus.hh"

#include <QMainWindow>

#include <cstdint>
#include <string>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;
class SimulationWorker;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(const std::string &aFirmwarePath,
             const std::string &aControlPath,
             std::uint32_t aFrequency,
             int aGdbPort,
             bool aWaitForGdb,
             QWidget *aParent = nullptr);
  ~MainWindow() override;

signals:
  void commandRequested(const QString &aCommand);
  void tapRequested(int aButton, int aMilliseconds);
  void holdRequested(int aButton);
  void releaseRequested();
  void stopRequested();

private slots:
  void simulationInitialized(bool aSuccess, const QString &aMessage);
  void statusChanged(const SimulatorStatus &aStatus);
  void appendLog(const QString &aMessage);

private:
  QWidget *createControls();
  QWidget *createFaultControls();
  QPushButton *createCommandButton(const QString &aText, const QString &aCommand);

  MachineWidget *mMachineWidget;
  QLabel *mConnectionLabel;
  QLabel *mCycleLabel;
  QLabel *mMotorLabel;
  QPlainTextEdit *mLog;
  QWidget *mControlsWidget;
  QWidget *mFaultControlsWidget;
  QThread *mSimulationThread;
  SimulationWorker *mWorker;
};

#endif
