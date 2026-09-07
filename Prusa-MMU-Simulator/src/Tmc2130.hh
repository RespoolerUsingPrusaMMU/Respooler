#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

class StepperMotor;

class Tmc2130 {
public:
    explicit Tmc2130(StepperMotor &motor);

    void chipSelect(bool active);
    std::uint8_t transfer(std::uint8_t out);
    bool selected() const { return selected_; }

    void setStepLevel(bool level) { stepLevel_ = level; }
    void setDirection(bool direction) { direction_ = direction; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    void setDriverError(bool on) { driverError_ = on; }
    void setOverTemperature(bool on) { overTemperature_ = on; }
    void setOverTemperaturePrewarn(bool on) { overTemperaturePrewarn_ = on; }
    void setUnderVoltage(bool on) { underVoltage_ = on; }
    void setPresent(bool on) { present_ = on; }
    void setValidIdentity(bool on) { validIdentity_ = on; }

    bool present() const { return present_; }
    bool validIdentity() const { return validIdentity_; }
    bool driverError() const { return driverError_; }
    bool overTemperature() const { return overTemperature_; }
    bool overTemperaturePrewarn() const { return overTemperaturePrewarn_; }
    bool underVoltage() const { return underVoltage_; }

    std::uint32_t registerValue(std::uint8_t address) const;

private:
    void finishDatagram();
    std::uint32_t readRegister(std::uint8_t address) const;
    void writeRegister(std::uint8_t address, std::uint32_t value);
    std::uint32_t ioInValue() const;
    std::uint32_t driverStatusValue() const;

    StepperMotor &motor_;
    bool selected_;
    bool stepLevel_;
    bool direction_;
    bool enabled_;
    bool driverError_;
    bool overTemperature_;
    bool overTemperaturePrewarn_;
    bool underVoltage_;
    bool present_;
    bool validIdentity_;

    std::size_t byteIndex_;
    std::array<std::uint8_t, 5> rx_;
    std::array<std::uint8_t, 5> tx_;
    std::array<std::uint8_t, 5> nextTx_;
    std::unordered_map<std::uint8_t, std::uint32_t> registers_;
};
