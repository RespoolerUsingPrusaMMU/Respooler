#include "MainWindow.hh"
#include "SimulationWorker.hh"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

MainWindow::MainWindow(const std::string &aFirmwarePath,
                       const std::string &aControlPath,
                       std::uint32_t aFrequency,
                       int aGdbPort,
                       bool aWaitForGdb,
                       QWidget *aParent)
  : QMainWindow(aParent),
    mMachineWidget(new MachineWidget(this)),
    mConnectionLabel(new QLabel("Starting simulator...", this)),
    mCycleLabel(new QLabel("Cycle: 0", this)),
    mMotorLabel(new QLabel(this)),
    mLog(new QPlainTextEdit(this)),
    mControlsWidget(nullptr),
    mFaultControlsWidget(nullptr),
    mSimulationThread(new QThread(this)),
    mWorker(new SimulationWorker(aFirmwarePath, aControlPath, aFrequency, aGdbPort, aWaitForGdb))
{
  setWindowTitle("Prusa MMU Spooler Simulator");
  resize(980, 820);

  QWidget *tCentral = new QWidget(this);
  QVBoxLayout *tMainLayout = new QVBoxLayout(tCentral);
  tMainLayout->addWidget(mMachineWidget);
  tMainLayout->addWidget(mConnectionLabel);
  tMainLayout->addWidget(mCycleLabel);
  tMainLayout->addWidget(mMotorLabel);
  mControlsWidget = createControls();
  mFaultControlsWidget = createFaultControls();
  mControlsWidget->setEnabled(false);
  mFaultControlsWidget->setEnabled(false);
  tMainLayout->addWidget(mControlsWidget);
  tMainLayout->addWidget(mFaultControlsWidget);

  mLog->setReadOnly(true);
  mLog->setMaximumBlockCount(1000);
  tMainLayout->addWidget(mLog, 1);
  setCentralWidget(tCentral);

  mWorker->moveToThread(mSimulationThread);
  connect(mSimulationThread, &QThread::started,
          mWorker, &SimulationWorker::initialize);
  connect(this, &MainWindow::commandRequested,
          mWorker, &SimulationWorker::executeCommand);
  connect(this, &MainWindow::tapRequested,
          mWorker, &SimulationWorker::tapButton);
  connect(this, &MainWindow::holdRequested,
          mWorker, &SimulationWorker::holdButton);
  connect(this, &MainWindow::releaseRequested,
          mWorker, &SimulationWorker::releaseButton);
  connect(this, &MainWindow::stopRequested,
          mWorker, &SimulationWorker::stop);
  connect(mWorker, &SimulationWorker::initialized,
          this, &MainWindow::simulationInitialized);
  connect(mWorker, &SimulationWorker::statusChanged,
          this, &MainWindow::statusChanged);
  connect(mWorker, &SimulationWorker::logMessage,
          this, &MainWindow::appendLog);
  connect(mWorker, &SimulationWorker::simulationFinished,
          this, [this](int aCode)
          {
            appendLog(QString("Simulation finished with code %1").arg(aCode));
          });
  connect(mSimulationThread, &QThread::finished, mWorker, &QObject::deleteLater);

  mSimulationThread->start();
}

MainWindow::~MainWindow()
{
  emit stopRequested();
  mSimulationThread->quit();
  mSimulationThread->wait();
}

QWidget *MainWindow::createControls()
{
  QGroupBox *tGroup = new QGroupBox("MMU Controls", this);
  QVBoxLayout *tOuter = new QVBoxLayout(tGroup);

  QHBoxLayout *tButtons = new QHBoxLayout();
  QPushButton *tLeft = new QPushButton("Left", tGroup);
  QPushButton *tMiddle = new QPushButton("Center / Start", tGroup);
  QPushButton *tRight = new QPushButton("Right", tGroup);
  tButtons->addWidget(tLeft);
  tButtons->addWidget(tMiddle);
  tButtons->addWidget(tRight);
  tOuter->addLayout(tButtons);

  connect(tLeft, &QPushButton::clicked, this,
          [this]() { emit tapRequested(static_cast<int>(MmuBoard::Button::Left), 500); });
  connect(tMiddle, &QPushButton::clicked, this,
          [this]() { emit tapRequested(static_cast<int>(MmuBoard::Button::Middle), 500); });
  connect(tRight, &QPushButton::clicked, this,
          [this]() { emit tapRequested(static_cast<int>(MmuBoard::Button::Right), 500); });

  QHBoxLayout *tSecond = new QHBoxLayout();
  tSecond->addWidget(createCommandButton("Filament Present", "filament present"));
  tSecond->addWidget(createCommandButton("Filament Absent", "filament absent"));
  tSecond->addWidget(createCommandButton("Re-home (5.5 s Center)", "tap middle 5500"));
  tSecond->addWidget(createCommandButton("Status to Terminal", "status"));
  tOuter->addLayout(tSecond);

  return tGroup;
}

QWidget *MainWindow::createFaultControls()
{
  QGroupBox *tGroup = new QGroupBox("Simulation / Fault Injection", this);
  QVBoxLayout *tOuter = new QVBoxLayout(tGroup);

  QHBoxLayout *tMechanics = new QHBoxLayout();
  tMechanics->addWidget(createCommandButton("Mechanics Auto", "mechanics auto"));
  tMechanics->addWidget(createCommandButton("Mechanics Manual", "mechanics manual"));
  tMechanics->addWidget(createCommandButton("Stall Shuttle", "stall shuttle"));
  tMechanics->addWidget(createCommandButton("Clear Shuttle Stall", "unstall shuttle"));
  tOuter->addLayout(tMechanics);

  QHBoxLayout *tTmc = new QHBoxLayout();
  tTmc->addWidget(createCommandButton("Shuttle TMC Missing", "tmc-present shuttle off"));
  tTmc->addWidget(createCommandButton("Shuttle TMC Present", "tmc-present shuttle on"));
  tTmc->addWidget(createCommandButton("Shuttle Bad ID", "tmc-id shuttle bad"));
  tTmc->addWidget(createCommandButton("Shuttle Good ID", "tmc-id shuttle good"));
  tOuter->addLayout(tTmc);

  QHBoxLayout *tThermal = new QHBoxLayout();
  tThermal->addWidget(createCommandButton("Take-up Overtemp", "tmc-overtemp takeup on"));
  tThermal->addWidget(createCommandButton("Clear Overtemp", "tmc-overtemp takeup off"));
  tThermal->addWidget(createCommandButton("Brake Undervoltage", "tmc-undervoltage brake on"));
  tThermal->addWidget(createCommandButton("Clear Undervoltage", "tmc-undervoltage brake off"));
  tOuter->addLayout(tThermal);

  return tGroup;
}

QPushButton *MainWindow::createCommandButton(const QString &aText,
                                              const QString &aCommand)
{
  QPushButton *tButton = new QPushButton(aText, this);
  connect(tButton, &QPushButton::clicked, this,
          [this, aCommand]() { emit commandRequested(aCommand); });
  return tButton;
}

void MainWindow::simulationInitialized(bool aSuccess, const QString &aMessage)
{
  mConnectionLabel->setText(aMessage);
  appendLog(aMessage);

  mControlsWidget->setEnabled(aSuccess);
  mFaultControlsWidget->setEnabled(aSuccess);

  if(!aSuccess)
  {
    mConnectionLabel->setStyleSheet("font-weight: bold; color: #b00020;");
  }
  else
  {
    mConnectionLabel->setStyleSheet(QString());
  }
}

void MainWindow::statusChanged(const SimulatorStatus &aStatus)
{
  mMachineWidget->setStatus(aStatus);
  mCycleLabel->setText(QString("AVR cycle: %1    Mechanics: %2")
                         .arg(static_cast<qulonglong>(aStatus.cycle))
                         .arg(aStatus.automaticMechanics ? "automatic" : "manual"));
  mMotorLabel->setText(QString("Shuttle steps: %1    Take-up steps: %2    Brake steps: %3")
                         .arg(static_cast<qulonglong>(aStatus.motors[0].stepCount))
                         .arg(static_cast<qulonglong>(aStatus.motors[1].stepCount))
                         .arg(static_cast<qulonglong>(aStatus.motors[2].stepCount)));
}

void MainWindow::appendLog(const QString &aMessage)
{
  mLog->appendPlainText(aMessage);
}
