#include "ControlInterface.hh"
#include "MmuBoard.hh"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void usage(const char *name) {
    std::cerr << "Usage: " << name << " [options] <firmware-elf>\n\n"
              << "Options:\n"
              << "  --gdb-port <port>    GDB server port (default 1234)\n"
              << "  --frequency <hz>     MCU frequency (default 16000000)\n"
              << "  --control <path>     Command FIFO (default /tmp/prusa-mmu-sim.cmd)\n"
              << "  --wait-for-gdb       Start AVR stopped until GDB continues it\n"
              << "  --help               Show this message\n";
}
}

int main(int argc, char **argv) {
    // Disable both C stdio buffering and C++ iostream buffering.  The
    // simulator is normally launched by VS Code with stdout/stderr
    // redirected to a log file, where the default buffering can make
    // successfully processed control commands appear to do nothing.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    std::uint32_t frequency = 16000000U;
    int gdbPort = 1234;
    std::string controlPath = "/tmp/prusa-mmu-sim.cmd";
    std::string firmwarePath;
    bool waitForGdb = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else if (arg == "--gdb-port" && i + 1 < argc) {
            gdbPort = std::atoi(argv[++i]);
        } else if (arg == "--frequency" && i + 1 < argc) {
            frequency = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0));
        } else if (arg == "--control" && i + 1 < argc) {
            controlPath = argv[++i];
        } else if (arg == "--wait-for-gdb") {
            waitForGdb = true;
        } else if (!arg.empty() && arg[0] != '-') {
            firmwarePath = arg;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (firmwarePath.empty()) {
        usage(argv[0]);
        return 1;
    }

    MmuBoard board;
    if (!board.loadFirmware(firmwarePath) || !board.initialize(frequency, gdbPort, waitForGdb))
        return 2;

    ControlInterface control(controlPath);
    if (!control.initialize())
        return 3;

    std::cout << "Prusa MMU / Spooler simulator\n"
              << "  MCU:       ATmega32U4\n"
              << "  Frequency: " << frequency << " Hz\n"
              << "  Firmware:  " << firmwarePath << "\n"
              << "  GDB:       localhost:" << gdbPort << "\n"
              << "  Control:   " << control.path() << "\n"
              << "\nSend commands with, for example:\n"
              << "  echo 'status' > " << control.path() << "\n"
              << "  echo 'filament present' > " << control.path() << "\n"
              << "  echo 'tap middle' > " << control.path() << "\n"
              << "  echo 'help' > " << control.path() << "\n\n";

    int state = cpu_Stopped;
    do {
        state = avr_run(board.avr());
        control.poll(board);
    } while (state != cpu_Done && state != cpu_Crashed);

    return state == cpu_Done ? 0 : 4;
}
