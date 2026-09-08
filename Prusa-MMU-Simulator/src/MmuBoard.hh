#pragma once

#include "ShiftRegister.hh"
#include "StepperMotor.hh"
#include "Tmc2130.hh"
#include "SimulatorStatus.hh"

#include <array>
#include <cstdint>
#include <string>

extern "C" {
#include "sim_avr.h"
#include "sim_elf.h"
}

class MmuBoard {
public:
    enum class Axis { Pulley = 0, Selector = 1, Idler = 2 };
    enum class Button { None, Left, Middle, Right };

    MmuBoard();
    ~MmuBoard();

    bool loadFirmware(const std::string &path);
    bool initialize(std::uint32_t frequencyHz = 16000000U,
                    int gdbPort = 1234,
                    bool waitForGdb = false);
    int run();

    void shiftRegisterLatched(std::uint16_t value);

    void setFinda(bool active);
    void setFilamentPresent(bool present);
    void setButton(Button button);
    void setStall(Axis axis, bool stalled);
    void setDriverError(Axis axis, bool on);
    void setDriverPresent(Axis axis, bool on);
    void setDriverIdentityValid(Axis axis, bool on);
    void setDriverUnderVoltage(Axis axis, bool on);
    void setDriverOverTemperature(Axis axis, bool on);
    void setDriverPrewarn(Axis axis, bool on);

    void setAutomaticMechanics(bool on);
    void setShuttlePosition(std::int64_t position);
    void setShuttleLimits(std::int64_t minimum, std::int64_t maximum);
    void setRunoutAfterTakeupSteps(std::uint64_t additionalSteps);
    void disableAutomaticRunout();

    std::uint32_t driverRegister(Axis axis, std::uint8_t address) const;
    SimulatorStatus statusSnapshot() const;
    void printStatus() const;

    avr_t *avr() const { return avr_; }

private:
    enum class Signal {
        ShrData,
        ShrClock,
        ShrLatch,
        PulleyStep,
        SelectorStep,
        IdlerStep,
        PulleyCs,
        SelectorCs,
        IdlerCs
    };

    struct PinCallbackContext {
        MmuBoard *board;
        Signal signal;
    };

    static void pinChanged(struct avr_irq_t *irq, std::uint32_t value, void *param);
    static void spiOutput(struct avr_irq_t *irq, std::uint32_t value, void *param);
    static void adcTrigger(struct avr_irq_t *irq, std::uint32_t value, void *param);

    void handlePin(Signal signal, bool level);
    std::uint8_t handleSpi(std::uint8_t value);
    void handleAdcTrigger(std::uint32_t value);

    void connectOutput(char port, unsigned pin, Signal signal, std::size_t contextIndex);
    void driveInput(char port, unsigned pin, bool level);
    void updateMechanicalState(Axis axis);
    void applyEffectiveStall(Axis axis);
    void checkAutomaticRunout();

    StepperMotor &motor(Axis axis);
    Tmc2130 &driver(Axis axis);
    const StepperMotor &motor(Axis axis) const;
    const Tmc2130 &driver(Axis axis) const;

    static std::size_t axisIndex(Axis axis);
    static std::uint16_t adcCountsForButton(Button button);
    static std::uint32_t adcCountsToMillivolts(std::uint16_t counts);

    avr_t *avr_;
    elf_firmware_t firmware_;
    bool firmwareLoaded_;
    bool initialized_;
    bool finda_;
    Button button_;
    std::uint16_t shiftValue_;

    StepperMotor pulley_;
    StepperMotor selector_;
    StepperMotor idler_;
    Tmc2130 pulleyDriver_;
    Tmc2130 selectorDriver_;
    Tmc2130 idlerDriver_;
    ShiftRegister16 shiftRegister_;

    std::array<bool, 3> manualStall_;
    std::array<bool, 3> automaticStall_;
    bool automaticMechanics_;
    std::int64_t shuttleMinimum_;
    std::int64_t shuttleMaximum_;
    bool automaticRunoutEnabled_;
    std::uint64_t automaticRunoutAtTakeupSteps_;

    std::array<PinCallbackContext, 9> pinContexts_;
};
