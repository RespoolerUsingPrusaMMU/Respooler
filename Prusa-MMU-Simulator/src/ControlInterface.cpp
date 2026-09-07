#include "ControlInterface.hh"
#include "MmuBoard.hh"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
static const std::uint32_t BUTTON_DEBOUNCE_PHASE_MS = 50U;
static const std::uint32_t DEFAULT_BUTTON_HOLD_MS = 100U;
static const std::uint32_t MINIMUM_BUTTON_HOLD_MS = 40U;

MmuBoard::Axis parseAxis(const std::string &name) {
    if (name == "selector" || name == "takeup")
        return MmuBoard::Axis::Selector;
    if (name == "idler" || name == "brake")
        return MmuBoard::Axis::Idler;
    return MmuBoard::Axis::Pulley;
}

bool parseOnOff(const std::string &value) {
    return value == "on" || value == "1" || value == "true" || value == "yes";
}

MmuBoard::Button parseButton(const std::string &name) {
    if (name == "left")
        return MmuBoard::Button::Left;
    if (name == "middle" || name == "start")
        return MmuBoard::Button::Middle;
    if (name == "right")
        return MmuBoard::Button::Right;
    return MmuBoard::Button::None;
}
}

ControlInterface::ControlInterface(const std::string &fifoPath)
    : fifoPath_(fifoPath), fd_(-1), tapState_(TapState::Idle),
      tapButtonValue_(static_cast<int>(MmuBoard::Button::None)),
      tapHoldMilliseconds_(DEFAULT_BUTTON_HOLD_MS), tapPhaseEndCycle_(0ULL) {}

ControlInterface::~ControlInterface() {
    if (fd_ >= 0)
        close(fd_);
    if (!fifoPath_.empty())
        unlink(fifoPath_.c_str());
}

bool ControlInterface::initialize() {
    unlink(fifoPath_.c_str());
    if (mkfifo(fifoPath_.c_str(), 0600) != 0) {
        std::cerr << "Unable to create control FIFO " << fifoPath_ << ": "
                  << std::strerror(errno) << "\n";
        return false;
    }

    fd_ = open(fifoPath_.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "Unable to open control FIFO " << fifoPath_ << ": "
                  << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

std::uint64_t ControlInterface::millisecondsToCycles(const MmuBoard &board,
                                                      std::uint32_t milliseconds) {
    const avr_t *avr = board.avr();
    if (!avr || avr->frequency == 0U)
        return 0ULL;

    return (static_cast<std::uint64_t>(milliseconds) *
            static_cast<std::uint64_t>(avr->frequency)) / 1000ULL;
}

const char *ControlInterface::buttonName(int buttonValue) {
    const MmuBoard::Button button = static_cast<MmuBoard::Button>(buttonValue);
    switch (button) {
    case MmuBoard::Button::Left:   return "left";
    case MmuBoard::Button::Middle: return "middle";
    case MmuBoard::Button::Right:  return "right";
    default:                       return "none";
    }
}

void ControlInterface::poll(MmuBoard &board) {
    if (fd_ < 0)
        return;

    // Advance an automatic button tap using simulated AVR CPU cycles.  This
    // makes debounce timing deterministic even when simavr runs faster or
    // slower than wall-clock time.
    updateButtonTap(board);

    char buffer[512];
    for (;;) {
        const ssize_t n = read(fd_, buffer, sizeof(buffer));
        if (n > 0) {
            pending_.append(buffer, static_cast<std::size_t>(n));
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            break;
        }
    }

    std::size_t newline = 0;
    while ((newline = pending_.find('\n')) != std::string::npos) {
        const std::string line = pending_.substr(0, newline);
        pending_.erase(0, newline + 1);

        if (!line.empty())
            std::cout << "[control] " << line << "\n";

        executeLine(board, line);
    }

    updateButtonTap(board);
}

void ControlInterface::startButtonTap(MmuBoard &board,
                                      const std::string &requestedButtonName,
                                      std::uint32_t holdMilliseconds) {
    const MmuBoard::Button button = parseButton(requestedButtonName);
    if (button == MmuBoard::Button::None) {
        std::cerr << "Unknown button for tap: " << requestedButtonName << "\n";
        return;
    }

    if (holdMilliseconds < MINIMUM_BUTTON_HOLD_MS)
        holdMilliseconds = MINIMUM_BUTTON_HOLD_MS;

    // Restarting a tap always begins with an explicit released phase.  This
    // guarantees the firmware can debounce Button::None and clear any prior
    // justPressed() bookkeeping before the requested button is asserted.
    board.setButton(MmuBoard::Button::None);
    tapButtonValue_ = static_cast<int>(button);
    tapHoldMilliseconds_ = holdMilliseconds;
    tapState_ = TapState::PreRelease;
    tapPhaseEndCycle_ = board.avr()->cycle +
        millisecondsToCycles(board, BUTTON_DEBOUNCE_PHASE_MS);

    std::cout << "Button tap " << requestedButtonName
              << " started: " << BUTTON_DEBOUNCE_PHASE_MS
              << " ms pre-release, " << holdMilliseconds
              << " ms press, " << BUTTON_DEBOUNCE_PHASE_MS
              << " ms post-release (simulated AVR time)\n";
}

void ControlInterface::updateButtonTap(MmuBoard &board) {
    if (tapState_ == TapState::Idle || !board.avr())
        return;

    const std::uint64_t currentCycle = board.avr()->cycle;
    if (currentCycle < tapPhaseEndCycle_)
        return;

    switch (tapState_) {
    case TapState::PreRelease:
        board.setButton(static_cast<MmuBoard::Button>(tapButtonValue_));
        tapState_ = TapState::Pressed;
        tapPhaseEndCycle_ = currentCycle +
            millisecondsToCycles(board, tapHoldMilliseconds_);
        std::cout << "Button " << buttonName(tapButtonValue_)
                  << " pressed\n";
        break;

    case TapState::Pressed:
        board.setButton(MmuBoard::Button::None);
        tapState_ = TapState::PostRelease;
        tapPhaseEndCycle_ = currentCycle +
            millisecondsToCycles(board, BUTTON_DEBOUNCE_PHASE_MS);
        std::cout << "Button " << buttonName(tapButtonValue_)
                  << " released\n";
        break;

    case TapState::PostRelease:
        tapState_ = TapState::Idle;
        tapButtonValue_ = static_cast<int>(MmuBoard::Button::None);
        tapPhaseEndCycle_ = 0ULL;
        std::cout << "Button tap complete\n";
        break;

    case TapState::Idle:
    default:
        break;
    }
}

void ControlInterface::cancelButtonTap(MmuBoard &board) {
    tapState_ = TapState::Idle;
    tapButtonValue_ = static_cast<int>(MmuBoard::Button::None);
    tapPhaseEndCycle_ = 0ULL;
    board.setButton(MmuBoard::Button::None);
}

void ControlInterface::executeLine(MmuBoard &board, const std::string &line) {
    std::istringstream in(line);
    std::string command;
    in >> command;
    if (command.empty())
        return;

    if (command == "status") {
        board.printStatus();
        return;
    }

    if (command == "finda") {
        std::string value;
        in >> value;
        board.setFinda(parseOnOff(value));
        return;
    }

    if (command == "filament") {
        std::string value;
        in >> value;
        board.setFilamentPresent(value == "present" || value == "on" || value == "1");
        return;
    }

    if (command == "tap" || command == "press") {
        std::string value;
        unsigned long holdMilliseconds = DEFAULT_BUTTON_HOLD_MS;
        in >> value;
        if (!(in >> holdMilliseconds))
            holdMilliseconds = DEFAULT_BUTTON_HOLD_MS;
        startButtonTap(board, value, static_cast<std::uint32_t>(holdMilliseconds));
        return;
    }

    if (command == "hold") {
        std::string value;
        in >> value;
        const MmuBoard::Button button = parseButton(value);
        if (button == MmuBoard::Button::None) {
            std::cerr << "Unknown button for hold: " << value << "\n";
            return;
        }
        cancelButtonTap(board);
        board.setButton(button);
        std::cout << "Button " << value << " held\n";
        return;
    }

    if (command == "release") {
        cancelButtonTap(board);
        std::cout << "Button released\n";
        return;
    }

    // Legacy manual button command retained for compatibility.
    if (command == "button") {
        std::string value;
        in >> value;
        cancelButtonTap(board);
        if (value == "left") board.setButton(MmuBoard::Button::Left);
        else if (value == "middle" || value == "start") board.setButton(MmuBoard::Button::Middle);
        else if (value == "right") board.setButton(MmuBoard::Button::Right);
        else board.setButton(MmuBoard::Button::None);
        return;
    }

    if (command == "stall" || command == "unstall") {
        std::string name;
        in >> name;
        board.setStall(parseAxis(name), command == "stall");
        return;
    }

    if (command == "driver-error" || command == "driver-ok") {
        std::string name;
        in >> name;
        board.setDriverError(parseAxis(name), command == "driver-error");
        return;
    }

    if (command == "tmc-present") {
        std::string name;
        std::string value;
        in >> name >> value;
        board.setDriverPresent(parseAxis(name), parseOnOff(value));
        return;
    }

    if (command == "tmc-id") {
        std::string name;
        std::string value;
        in >> name >> value;
        board.setDriverIdentityValid(parseAxis(name), value == "good" || value == "valid");
        return;
    }

    if (command == "tmc-undervoltage") {
        std::string name;
        std::string value;
        in >> name >> value;
        board.setDriverUnderVoltage(parseAxis(name), parseOnOff(value));
        return;
    }

    if (command == "tmc-overtemp") {
        std::string name;
        std::string value;
        in >> name >> value;
        board.setDriverOverTemperature(parseAxis(name), parseOnOff(value));
        return;
    }

    if (command == "tmc-prewarn") {
        std::string name;
        std::string value;
        in >> name >> value;
        board.setDriverPrewarn(parseAxis(name), parseOnOff(value));
        return;
    }

    if (command == "tmc-reg") {
        std::string name;
        std::string addressText;
        in >> name >> addressText;
        const unsigned long address = std::strtoul(addressText.c_str(), nullptr, 0);
        const std::uint32_t value = board.driverRegister(
            parseAxis(name), static_cast<std::uint8_t>(address & 0xffUL));
        std::cout << name << " register " << addressText << " = 0x"
                  << std::hex << std::setw(8) << std::setfill('0') << value
                  << std::dec << std::setfill(' ') << "\n";
        return;
    }

    if (command == "mechanics") {
        std::string value;
        in >> value;
        board.setAutomaticMechanics(value != "manual" && value != "off");
        return;
    }

    if (command == "shuttle-position") {
        long long position = 0;
        in >> position;
        board.setShuttlePosition(static_cast<std::int64_t>(position));
        return;
    }

    if (command == "shuttle-limits") {
        long long minimum = 0;
        long long maximum = 0;
        in >> minimum >> maximum;
        board.setShuttleLimits(static_cast<std::int64_t>(minimum),
                               static_cast<std::int64_t>(maximum));
        return;
    }

    if (command == "runout") {
        std::string value;
        in >> value;
        if (value == "off" || value == "disable") {
            board.disableAutomaticRunout();
        } else if (value == "after") {
            unsigned long long steps = 0;
            in >> steps;
            board.setRunoutAfterTakeupSteps(static_cast<std::uint64_t>(steps));
        }
        return;
    }

    if (command == "help") {
        std::cout
            << "Commands:\n"
            << "  status\n"
            << "  filament present|absent\n"
            << "  finda on|off                          (raw PF6 level)\n"
            << "  tap left|middle|right [milliseconds] (debounced one-shot; default press 100 ms)\n"
            << "  press left|middle|right [milliseconds] (alias for tap)\n"
            << "  hold left|middle|right               (manual sustained press)\n"
            << "  release                              (release held button/cancel tap)\n"
            << "  button left|middle|right|release     (legacy manual form)\n"
            << "  mechanics auto|manual\n"
            << "  shuttle-position <physical-steps>\n"
            << "  shuttle-limits <min-steps> <max-steps>\n"
            << "  stall shuttle|takeup|brake\n"
            << "  unstall shuttle|takeup|brake\n"
            << "  runout after <takeup-steps>\n"
            << "  runout off\n"
            << "  tmc-present shuttle|takeup|brake on|off\n"
            << "  tmc-id shuttle|takeup|brake good|bad\n"
            << "  driver-error shuttle|takeup|brake\n"
            << "  driver-ok shuttle|takeup|brake\n"
            << "  tmc-undervoltage shuttle|takeup|brake on|off\n"
            << "  tmc-prewarn shuttle|takeup|brake on|off\n"
            << "  tmc-overtemp shuttle|takeup|brake on|off\n"
            << "  tmc-reg shuttle|takeup|brake <address>\n";
        return;
    }

    std::cerr << "Unknown simulator command: " << line << "\n";
}
