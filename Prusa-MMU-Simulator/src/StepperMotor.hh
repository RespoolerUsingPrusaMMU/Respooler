#pragma once

#include <cstdint>
#include <string>

class StepperMotor {
public:
    explicit StepperMotor(const std::string &name);

    void setEnabled(bool enabled);
    void setDirection(bool direction);
    void setStepLevel(bool level);
    void setStalled(bool stalled);
    void setPosition(std::int64_t position);

    bool enabled() const { return enabled_; }
    bool direction() const { return direction_; }
    bool stalled() const { return stalled_; }
    bool stepLevel() const { return stepLevel_; }
    std::int64_t position() const { return position_; }
    std::uint64_t stepCount() const { return stepCount_; }
    const std::string &name() const { return name_; }

private:
    std::string name_;
    bool enabled_;
    bool direction_;
    bool stalled_;
    bool stepLevel_;
    std::int64_t position_;
    std::uint64_t stepCount_;
};
