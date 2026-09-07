#include "Tmc2130.hh"
#include "StepperMotor.hh"

namespace {
static const std::uint8_t REG_GCONF       = 0x00;
static const std::uint8_t REG_GSTAT       = 0x01;
static const std::uint8_t REG_IOIN        = 0x04;
static const std::uint8_t REG_IHOLD_IRUN  = 0x10;
static const std::uint8_t REG_TPOWERDOWN  = 0x11;
static const std::uint8_t REG_TPWMTHRS    = 0x13;
static const std::uint8_t REG_TCOOLTHRS   = 0x14;
static const std::uint8_t REG_CHOPCONF    = 0x6c;
static const std::uint8_t REG_COOLCONF    = 0x6d;
static const std::uint8_t REG_DRV_STATUS  = 0x6f;
static const std::uint8_t REG_PWMCONF     = 0x70;

static const std::uint32_t IOIN_VERSION = 0x11UL << 24;
static const std::uint32_t IOIN_VARIANT_BIT = 1UL << 6;
}

Tmc2130::Tmc2130(StepperMotor &motor)
    : motor_(motor), selected_(false), stepLevel_(false), direction_(false),
      enabled_(false), driverError_(false), overTemperature_(false),
      overTemperaturePrewarn_(false), underVoltage_(false), present_(true),
      validIdentity_(true), byteIndex_(0) {
    rx_.fill(0);
    tx_.fill(0);
    nextTx_.fill(0);
}

void Tmc2130::chipSelect(bool active) {
    if (active && !selected_) {
        byteIndex_ = 0;
        rx_.fill(0);
        tx_ = nextTx_;       // TMC2130 returns the previous read request.
        nextTx_.fill(0);
    } else if (!active && selected_) {
        if (byteIndex_ == 5)
            finishDatagram();
    }
    selected_ = active;
}

std::uint8_t Tmc2130::transfer(std::uint8_t out) {
    if (!selected_ || !present_)
        return 0xff;

    const std::size_t i = byteIndex_ < 5 ? byteIndex_ : 4;
    const std::uint8_t in = tx_[i];
    if (byteIndex_ < 5)
        rx_[byteIndex_++] = out;
    return in;
}

void Tmc2130::finishDatagram() {
    if (!present_)
        return;

    const std::uint8_t address = static_cast<std::uint8_t>(rx_[0] & 0x7fU);
    const bool write = (rx_[0] & 0x80U) != 0;

    if (write) {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(rx_[1]) << 24) |
            (static_cast<std::uint32_t>(rx_[2]) << 16) |
            (static_cast<std::uint32_t>(rx_[3]) << 8) |
            static_cast<std::uint32_t>(rx_[4]);
        writeRegister(address, value);
    } else {
        const std::uint32_t value = readRegister(address);
        nextTx_[0] = driverError_ ? 0x02U : 0x00U;
        nextTx_[1] = static_cast<std::uint8_t>(value >> 24);
        nextTx_[2] = static_cast<std::uint8_t>(value >> 16);
        nextTx_[3] = static_cast<std::uint8_t>(value >> 8);
        nextTx_[4] = static_cast<std::uint8_t>(value);
    }
}

std::uint32_t Tmc2130::registerValue(std::uint8_t address) const {
    return readRegister(address);
}

std::uint32_t Tmc2130::readRegister(std::uint8_t address) const {
    if (!present_)
        return 0xffffffffUL;
    if (address == REG_GSTAT)
        return underVoltage_ ? (1UL << 2) : 0UL;
    if (address == REG_IOIN)
        return ioInValue();
    if (address == REG_DRV_STATUS)
        return driverStatusValue();

    const auto it = registers_.find(address);
    return it == registers_.end() ? 0UL : it->second;
}

void Tmc2130::writeRegister(std::uint8_t address, std::uint32_t value) {
    // GSTAT is write-one-to-clear on the real part.  The current spooler only
    // reads it, but implementing the clearing behavior makes fault tests sane.
    if (address == REG_GSTAT) {
        if ((value & (1UL << 2)) != 0)
            underVoltage_ = false;
        return;
    }

    registers_[address] = value;
}

std::uint32_t Tmc2130::ioInValue() const {
    // The standalone spooler validates VERSION=0x11 in bits 31:24 and requires
    // bit 6 high before it will configure a channel.  Preserve the live STEP,
    // DIR, and active-low enable observations in the low bits as well.
    std::uint32_t v = validIdentity_ ? (IOIN_VERSION | IOIN_VARIANT_BIT) : 0UL;
    if (stepLevel_) v |= (1UL << 0);
    if (direction_) v |= (1UL << 1);
    if (!enabled_)  v |= (1UL << 4);
    return v;
}

std::uint32_t Tmc2130::driverStatusValue() const {
    std::uint32_t v = 0;
    if (driverError_)            v |= (3UL << 27); // simulated short flags
    if (overTemperaturePrewarn_) v |= (1UL << 26);
    if (overTemperature_)        v |= (1UL << 25);
    return v;
}
