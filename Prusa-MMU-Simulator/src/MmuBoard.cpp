#include "MmuBoard.hh"

#include <cstring>
#include <iomanip>
#include <iostream>

extern "C" {
#include "avr_adc.h"
#include "avr_ioport.h"
#include "avr_spi.h"
#include "sim_gdb.h"
}

namespace {
static const std::uint16_t SHR_DIR_PULLEY = 0x0001;
static const std::uint16_t SHR_ENA_PULLEY = 0x0002;
static const std::uint16_t SHR_DIR_SELECTOR = 0x0004;
static const std::uint16_t SHR_ENA_SELECTOR = 0x0008;
static const std::uint16_t SHR_DIR_IDLER = 0x0010;
static const std::uint16_t SHR_ENA_IDLER = 0x0020;

static const std::uint16_t LED_GREEN[5] = {0x0040, 0x4000, 0x1000, 0x0400, 0x0100};
static const std::uint16_t LED_RED[5]   = {0x0080, 0x8000, 0x2000, 0x0800, 0x0200};

// The spooler uses approximately 83.56 shuttle microsteps/mm.  The default
// mechanical limits therefore give a little more than 60 mm of usable travel.
static const std::int64_t DEFAULT_SHUTTLE_START = 1000;
static const std::int64_t DEFAULT_SHUTTLE_MINIMUM = 0;
static const std::int64_t DEFAULT_SHUTTLE_MAXIMUM = 5200;
}

MmuBoard::MmuBoard()
    : avr_(nullptr), firmwareLoaded_(false), initialized_(false), finda_(false),
      button_(Button::None), shiftValue_(0), pulley_("pulley/shuttle"),
      selector_("selector/takeup"), idler_("idler/brake"),
      pulleyDriver_(pulley_), selectorDriver_(selector_), idlerDriver_(idler_),
      shiftRegister_(*this), automaticMechanics_(true),
      shuttleMinimum_(DEFAULT_SHUTTLE_MINIMUM),
      shuttleMaximum_(DEFAULT_SHUTTLE_MAXIMUM), automaticRunoutEnabled_(false),
      automaticRunoutAtTakeupSteps_(0) {
    std::memset(&firmware_, 0, sizeof(firmware_));
    manualStall_.fill(false);
    automaticStall_.fill(false);
    pulley_.setPosition(DEFAULT_SHUTTLE_START);
}

MmuBoard::~MmuBoard() {
    if (avr_)
        avr_terminate(avr_);
}

bool MmuBoard::loadFirmware(const std::string &path) {
    std::memset(&firmware_, 0, sizeof(firmware_));
    if (elf_read_firmware(path.c_str(), &firmware_) != 0) {
        std::cerr << "Unable to read firmware ELF: " << path << "\n";
        return false;
    }
    firmwareLoaded_ = true;
    return true;
}

bool MmuBoard::initialize(std::uint32_t frequencyHz, int gdbPort, bool waitForGdb) {
    if (!firmwareLoaded_) {
        std::cerr << "Firmware must be loaded before board initialization.\n";
        return false;
    }

    avr_ = avr_make_mcu_by_name("atmega32u4");
    if (!avr_) {
        std::cerr << "simavr does not contain the atmega32u4 core.\n";
        return false;
    }

    avr_init(avr_);
    avr_->frequency = frequencyHz;
    avr_->vcc = 5000;
    avr_->avcc = 5000;
    avr_->aref = 5000;
    avr_load_firmware(avr_, &firmware_);

    // ATmega32U4 pins used by the standalone spooler firmware.
    connectOutput('B', 5, Signal::ShrData,      0); // 74HC595 DS
    connectOutput('C', 7, Signal::ShrClock,     1); // 74HC595 SHCP
    connectOutput('B', 6, Signal::ShrLatch,     2); // 74HC595 STCP
    connectOutput('B', 4, Signal::PulleyStep,   3); // shuttle
    connectOutput('D', 4, Signal::SelectorStep, 4); // take-up
    connectOutput('D', 6, Signal::IdlerStep,    5); // brake
    connectOutput('C', 6, Signal::PulleyCs,     6);
    connectOutput('D', 7, Signal::SelectorCs,   7);
    connectOutput('B', 7, Signal::IdlerCs,      8);

    avr_irq_t *spiOut = avr_io_getirq(avr_, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT);
    if (spiOut)
        avr_irq_register_notify(spiOut, &MmuBoard::spiOutput, this);

    avr_irq_t *adcOut = avr_io_getirq(avr_, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_OUT_TRIGGER);
    if (adcOut)
        avr_irq_register_notify(adcOut, &MmuBoard::adcTrigger, this);

    // FINDA low means filament present in the spooler configuration.
    setFilamentPresent(true);
    setButton(Button::None);
    applyEffectiveStall(Axis::Pulley);
    applyEffectiveStall(Axis::Selector);
    applyEffectiveStall(Axis::Idler);

    avr_->gdb_port = gdbPort;
    avr_gdb_init(avr_);

    // Normal simulator use should begin executing firmware immediately.
    // VS Code debugging can request a stopped CPU so avr-gdb can attach
    // before the first firmware instruction executes.
    avr_->state = waitForGdb ? cpu_Stopped : cpu_Running;

    initialized_ = true;
    return true;
}

int MmuBoard::run() {
    if (!initialized_)
        return 1;

    int state = cpu_Stopped;
    do {
        state = avr_run(avr_);
    } while (state != cpu_Done && state != cpu_Crashed);
    return state == cpu_Done ? 0 : 2;
}

void MmuBoard::connectOutput(char port, unsigned pin, Signal signal, std::size_t contextIndex) {
    pinContexts_[contextIndex].board = this;
    pinContexts_[contextIndex].signal = signal;
    avr_irq_t *irq = avr_io_getirq(avr_, AVR_IOCTL_IOPORT_GETIRQ(port), pin);
    if (irq)
        avr_irq_register_notify(irq, &MmuBoard::pinChanged, &pinContexts_[contextIndex]);
}

void MmuBoard::driveInput(char port, unsigned pin, bool level) {
    if (!avr_)
        return;
    avr_irq_t *irq = avr_io_getirq(avr_, AVR_IOCTL_IOPORT_GETIRQ(port), pin);
    if (irq)
        avr_raise_irq(irq, level ? 1U : 0U);
}

void MmuBoard::pinChanged(struct avr_irq_t *, std::uint32_t value, void *param) {
    PinCallbackContext *ctx = static_cast<PinCallbackContext *>(param);
    ctx->board->handlePin(ctx->signal, (value & 1U) != 0);
}

void MmuBoard::handlePin(Signal signal, bool level) {
    switch (signal) {
    case Signal::ShrData:
        shiftRegister_.setData(level);
        break;
    case Signal::ShrClock:
        shiftRegister_.setClock(level);
        break;
    case Signal::ShrLatch:
        shiftRegister_.setLatch(level);
        break;
    case Signal::PulleyStep:
        pulley_.setStepLevel(level);
        pulleyDriver_.setStepLevel(level);
        updateMechanicalState(Axis::Pulley);
        break;
    case Signal::SelectorStep:
        selector_.setStepLevel(level);
        selectorDriver_.setStepLevel(level);
        updateMechanicalState(Axis::Selector);
        checkAutomaticRunout();
        break;
    case Signal::IdlerStep:
        idler_.setStepLevel(level);
        idlerDriver_.setStepLevel(level);
        updateMechanicalState(Axis::Idler);
        break;
    case Signal::PulleyCs:
        pulleyDriver_.chipSelect(!level);
        break;
    case Signal::SelectorCs:
        selectorDriver_.chipSelect(!level);
        break;
    case Signal::IdlerCs:
        idlerDriver_.chipSelect(!level);
        break;
    }
}

void MmuBoard::spiOutput(struct avr_irq_t *, std::uint32_t value, void *param) {
    MmuBoard *board = static_cast<MmuBoard *>(param);
    const std::uint8_t response = board->handleSpi(static_cast<std::uint8_t>(value));
    avr_irq_t *spiIn = avr_io_getirq(board->avr_, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT);
    if (spiIn)
        avr_raise_irq(spiIn, response);
}

std::uint8_t MmuBoard::handleSpi(std::uint8_t value) {
    if (pulleyDriver_.selected())   return pulleyDriver_.transfer(value);
    if (selectorDriver_.selected()) return selectorDriver_.transfer(value);
    if (idlerDriver_.selected())    return idlerDriver_.transfer(value);
    return 0xff;
}

void MmuBoard::adcTrigger(struct avr_irq_t *, std::uint32_t value, void *param) {
    static_cast<MmuBoard *>(param)->handleAdcTrigger(value);
}

void MmuBoard::handleAdcTrigger(std::uint32_t value) {
    (void)value;
    if (avr_ == nullptr)
        return;

    avr_irq_t *adc5 = avr_io_getirq(avr_, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC5);
    if (adc5)
        avr_raise_irq(adc5, adcCountsToMillivolts(adcCountsForButton(button_)));
}

void MmuBoard::shiftRegisterLatched(std::uint16_t value) {
    shiftValue_ = value;

    const bool pulleyDir = (value & SHR_DIR_PULLEY) != 0;
    const bool selectorDir = (value & SHR_DIR_SELECTOR) != 0;
    const bool idlerDir = (value & SHR_DIR_IDLER) != 0;
    const bool pulleyEnabled = (value & SHR_ENA_PULLEY) == 0;
    const bool selectorEnabled = (value & SHR_ENA_SELECTOR) == 0;
    const bool idlerEnabled = (value & SHR_ENA_IDLER) == 0;

    // Convert the electrical shift-register DIR levels back into the logical
    // motion directions used by the standalone spooler.  SHR16::SetTMCDir()
    // itself writes an inverted bit; the firmware additionally marks the
    // take-up and brake axes as direction-inverted.
    pulley_.setDirection(!pulleyDir);
    selector_.setDirection(selectorDir);
    idler_.setDirection(idlerDir);
    pulley_.setEnabled(pulleyEnabled);
    selector_.setEnabled(selectorEnabled);
    idler_.setEnabled(idlerEnabled);

    pulleyDriver_.setDirection(pulleyDir);
    selectorDriver_.setDirection(selectorDir);
    idlerDriver_.setDirection(idlerDir);
    pulleyDriver_.setEnabled(pulleyEnabled);
    selectorDriver_.setEnabled(selectorEnabled);
    idlerDriver_.setEnabled(idlerEnabled);

    // A shuttle held at the home stop must be allowed to move away as soon as
    // firmware reverses DIR.  Reevaluate auto StallGuard on every DIR update.
    updateMechanicalState(Axis::Pulley);
}

void MmuBoard::setFinda(bool active) {
    finda_ = active;
    driveInput('F', 6, active);
}

void MmuBoard::setFilamentPresent(bool present) {
    // Current spooler setting: FINDA_HIGH_MEANS_NO_FILAMENT = true.
    setFinda(!present);
}

void MmuBoard::setButton(Button button) {
    button_ = button;

    // GUI commands can arrive after a failed initialization or after the
    // simulated CPU has stopped.  Never call into simavr without a valid AVR.
    if (avr_ == nullptr)
        return;

    avr_irq_t *adc5 = avr_io_getirq(avr_, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC5);
    if (adc5)
        avr_raise_irq(adc5, adcCountsToMillivolts(adcCountsForButton(button_)));
}

std::size_t MmuBoard::axisIndex(Axis axis) {
    return static_cast<std::size_t>(axis);
}

void MmuBoard::setStall(Axis axis, bool stalled) {
    manualStall_[axisIndex(axis)] = stalled;
    applyEffectiveStall(axis);
}

void MmuBoard::applyEffectiveStall(Axis axis) {
    const bool stalled = manualStall_[axisIndex(axis)] || automaticStall_[axisIndex(axis)];
    motor(axis).setStalled(stalled);

    // The spooler reads DIAG/StallGuard as active-low.
    switch (axis) {
    case Axis::Pulley:   driveInput('F', 4, !stalled); break;
    case Axis::Selector: driveInput('F', 1, !stalled); break;
    case Axis::Idler:    driveInput('F', 0, !stalled); break;
    }
}

void MmuBoard::updateMechanicalState(Axis axis) {
    if (axis != Axis::Pulley || !automaticMechanics_) {
        applyEffectiveStall(axis);
        return;
    }

    StepperMotor &shuttle = pulley_;
    bool automaticStall = false;

    if (shuttle.position() <= shuttleMinimum_) {
        shuttle.setPosition(shuttleMinimum_);
        // Logical negative direction is the homing/inward direction.
        automaticStall = !shuttle.direction();
    } else if (shuttle.position() >= shuttleMaximum_) {
        shuttle.setPosition(shuttleMaximum_);
        automaticStall = shuttle.direction();
    }

    automaticStall_[axisIndex(axis)] = automaticStall;
    applyEffectiveStall(axis);
}

void MmuBoard::setDriverError(Axis axis, bool on) {
    driver(axis).setDriverError(on);
}

void MmuBoard::setDriverPresent(Axis axis, bool on) {
    driver(axis).setPresent(on);
}

void MmuBoard::setDriverIdentityValid(Axis axis, bool on) {
    driver(axis).setValidIdentity(on);
}

void MmuBoard::setDriverUnderVoltage(Axis axis, bool on) {
    driver(axis).setUnderVoltage(on);
}

void MmuBoard::setDriverOverTemperature(Axis axis, bool on) {
    driver(axis).setOverTemperature(on);
}

void MmuBoard::setDriverPrewarn(Axis axis, bool on) {
    driver(axis).setOverTemperaturePrewarn(on);
}

void MmuBoard::setAutomaticMechanics(bool on) {
    automaticMechanics_ = on;
    if (!on)
        automaticStall_[axisIndex(Axis::Pulley)] = false;
    updateMechanicalState(Axis::Pulley);
}

void MmuBoard::setShuttlePosition(std::int64_t position) {
    pulley_.setPosition(position);
    updateMechanicalState(Axis::Pulley);
}

void MmuBoard::setShuttleLimits(std::int64_t minimum, std::int64_t maximum) {
    if (minimum >= maximum)
        return;
    shuttleMinimum_ = minimum;
    shuttleMaximum_ = maximum;
    updateMechanicalState(Axis::Pulley);
}

void MmuBoard::setRunoutAfterTakeupSteps(std::uint64_t additionalSteps) {
    automaticRunoutAtTakeupSteps_ = selector_.stepCount() + additionalSteps;
    automaticRunoutEnabled_ = true;
    setFilamentPresent(true);
}

void MmuBoard::disableAutomaticRunout() {
    automaticRunoutEnabled_ = false;
}

void MmuBoard::checkAutomaticRunout() {
    if (automaticRunoutEnabled_ && selector_.stepCount() >= automaticRunoutAtTakeupSteps_) {
        automaticRunoutEnabled_ = false;
        setFilamentPresent(false);
        std::cout << "Simulator: automatic filament runout asserted after "
                  << selector_.stepCount() << " take-up steps.\n";
    }
}

StepperMotor &MmuBoard::motor(Axis axis) {
    switch (axis) {
    case Axis::Pulley: return pulley_;
    case Axis::Selector: return selector_;
    default: return idler_;
    }
}

const StepperMotor &MmuBoard::motor(Axis axis) const {
    switch (axis) {
    case Axis::Pulley: return pulley_;
    case Axis::Selector: return selector_;
    default: return idler_;
    }
}

Tmc2130 &MmuBoard::driver(Axis axis) {
    switch (axis) {
    case Axis::Pulley: return pulleyDriver_;
    case Axis::Selector: return selectorDriver_;
    default: return idlerDriver_;
    }
}

const Tmc2130 &MmuBoard::driver(Axis axis) const {
    switch (axis) {
    case Axis::Pulley: return pulleyDriver_;
    case Axis::Selector: return selectorDriver_;
    default: return idlerDriver_;
    }
}

std::uint32_t MmuBoard::driverRegister(Axis axis, std::uint8_t address) const {
    return driver(axis).registerValue(address);
}

std::uint16_t MmuBoard::adcCountsForButton(Button button) {
    // Match the standalone spooler Defaults.hh exactly:
    // RIGHT=0..50, MIDDLE=80..100, LEFT=160..180.
    switch (button) {
    case Button::Right:  return 25;
    case Button::Middle: return 90;
    case Button::Left:   return 170;
    default:             return 1023;
    }
}

std::uint32_t MmuBoard::adcCountsToMillivolts(std::uint16_t counts) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(counts) * 5000ULL) / 1023ULL);
}

SimulatorStatus MmuBoard::statusSnapshot() const {
    SimulatorStatus status{};
    status.initialized = initialized_;
    status.filamentPresent = !finda_;
    status.automaticMechanics = automaticMechanics_;
    status.shuttleMinimum = shuttleMinimum_;
    status.shuttleMaximum = shuttleMaximum_;
    status.cycle = avr_ ? avr_->cycle : 0ULL;

    const Axis axes[] = {Axis::Pulley, Axis::Selector, Axis::Idler};
    for (std::size_t index = 0; index < 3; ++index) {
        const StepperMotor &sourceMotor = motor(axes[index]);
        const Tmc2130 &sourceDriver = driver(axes[index]);
        MotorStatus &target = status.motors[index];
        target.name = sourceMotor.name();
        target.enabled = sourceMotor.enabled();
        target.direction = sourceMotor.direction();
        target.stalled = sourceMotor.stalled();
        target.position = sourceMotor.position();
        target.stepCount = sourceMotor.stepCount();
        target.driverPresent = sourceDriver.present();
        target.driverIdentityValid = sourceDriver.validIdentity();
        target.underVoltage = sourceDriver.underVoltage();
        target.overTemperature = sourceDriver.overTemperature();
        target.overTemperaturePrewarn = sourceDriver.overTemperaturePrewarn();
    }

    for (std::size_t index = 0; index < 5; ++index) {
        status.redLeds[index] = (shiftValue_ & LED_RED[index]) != 0;
        status.greenLeds[index] = (shiftValue_ & LED_GREEN[index]) != 0;
    }

    return status;
}

void MmuBoard::printStatus() const {
    std::cout << "\nSpooler simulator status\n"
              << "  FINDA raw:      " << (finda_ ? "HIGH (no filament)" : "LOW (filament present)") << "\n"
              << "  Mechanics:      " << (automaticMechanics_ ? "automatic" : "manual") << "\n"
              << "  Shuttle limits: [" << shuttleMinimum_ << ", " << shuttleMaximum_ << "] steps\n";

    const char *buttonName = "none";
    if (button_ == Button::Left) buttonName = "left";
    else if (button_ == Button::Middle) buttonName = "middle";
    else if (button_ == Button::Right) buttonName = "right";
    std::cout << "  Button:         " << buttonName << "\n";

    const Axis axes[] = {Axis::Pulley, Axis::Selector, Axis::Idler};
    for (Axis axis : axes) {
        const StepperMotor &m = motor(axis);
        const Tmc2130 &d = driver(axis);
        std::cout << "  " << std::setw(16) << std::left << m.name()
                  << " enabled=" << (m.enabled() ? "yes" : "no")
                  << " dir=" << (m.direction() ? "1" : "0")
                  << " stall=" << (m.stalled() ? "yes" : "no")
                  << " position=" << m.position()
                  << " steps=" << m.stepCount()
                  << " tmc=" << (d.present() ? "present" : "missing")
                  << " id=" << (d.validIdentity() ? "good" : "bad")
                  << "\n";
    }

    std::cout << "  TMC IOIN:       shuttle=0x" << std::hex << std::setw(8) << std::setfill('0')
              << driverRegister(Axis::Pulley, 0x04)
              << " takeup=0x" << std::setw(8) << driverRegister(Axis::Selector, 0x04)
              << " brake=0x" << std::setw(8) << driverRegister(Axis::Idler, 0x04)
              << std::dec << std::setfill(' ') << "\n";

    if (automaticRunoutEnabled_)
        std::cout << "  Auto runout at: " << automaticRunoutAtTakeupSteps_ << " take-up steps\n";
    else
        std::cout << "  Auto runout:    off\n";

    std::cout << "  LEDs:";
    for (unsigned i = 0; i < 5; ++i) {
        const bool r = (shiftValue_ & LED_RED[i]) != 0;
        const bool g = (shiftValue_ & LED_GREEN[i]) != 0;
        const char *state = r && g ? "RG" : r ? "R" : g ? "G" : "-";
        std::cout << " [" << i << ':' << state << ']';
    }
    std::cout << "\n\n";
}
