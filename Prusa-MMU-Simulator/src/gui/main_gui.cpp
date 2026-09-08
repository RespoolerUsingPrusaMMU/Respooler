#include "MainWindow.hh"
#include "SimulatorStatus.hh"

#include <QApplication>
#include <QMetaType>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
void usage(const char *aName)
{
  std::cerr << "Usage: " << aName << " [options] <firmware-elf>\n\n"
            << "Options:\n"
            << "  --gdb-port <port>    GDB server port (default 1234)\n"
            << "  --frequency <hz>     MCU frequency (default 16000000)\n"
            << "  --control <path>     Command FIFO (default /tmp/prusa-mmu-sim.cmd)\n"
            << "  --wait-for-gdb       Start AVR stopped until GDB continues it\n"
            << "  --help               Show this message\n";
}
}

int main(int argc, char **argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::cout.setf(std::ios::unitbuf);
  std::cerr.setf(std::ios::unitbuf);

  std::uint32_t tFrequency = 16000000U;
  int tGdbPort = 1234;
  std::string tControlPath = "/tmp/prusa-mmu-sim.cmd";
  std::string tFirmwarePath;
  bool tWaitForGdb = false;

  for(int tArgIndex = 1; tArgIndex < argc; ++tArgIndex)
  {
    const std::string tArg(argv[tArgIndex]);
    if(tArg == "--help" || tArg == "-h")
    {
      usage(argv[0]);
      return 0;
    }
    else if(tArg == "--gdb-port" && tArgIndex + 1 < argc)
    {
      tGdbPort = std::atoi(argv[++tArgIndex]);
    }
    else if(tArg == "--frequency" && tArgIndex + 1 < argc)
    {
      tFrequency = static_cast<std::uint32_t>(std::strtoul(argv[++tArgIndex], nullptr, 0));
    }
    else if(tArg == "--control" && tArgIndex + 1 < argc)
    {
      tControlPath = argv[++tArgIndex];
    }
    else if(tArg == "--wait-for-gdb")
    {
      tWaitForGdb = true;
    }
    else if(!tArg.empty() && tArg[0] != '-')
    {
      tFirmwarePath = tArg;
    }
    else
    {
      usage(argv[0]);
      return 1;
    }
  }

  if(tFirmwarePath.empty())
  {
    usage(argv[0]);
    return 1;
  }

  QApplication tApplication(argc, argv);
  qRegisterMetaType<SimulatorStatus>("SimulatorStatus");

  std::cout << "Prusa MMU / Spooler Qt simulator\n"
            << "  MCU:       ATmega32U4\n"
            << "  Frequency: " << tFrequency << " Hz\n"
            << "  Firmware:  " << tFirmwarePath << "\n"
            << "  GDB:       localhost:" << tGdbPort << "\n"
            << "  Control:   " << tControlPath << "\n"
            << "  Start:     " << (tWaitForGdb ? "wait for GDB" : "run immediately") << "\n";

  MainWindow tWindow(tFirmwarePath, tControlPath, tFrequency, tGdbPort, tWaitForGdb);
  tWindow.show();

  return tApplication.exec();
}
