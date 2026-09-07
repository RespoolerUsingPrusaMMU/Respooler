#include "StepperMotor.hh"

StepperMotor::StepperMotor(const std::string &name)
    : name_(name), enabled_(false), direction_(false), stalled_(false),
      stepLevel_(false), position_(0), stepCount_(0) {}

void StepperMotor::setEnabled(bool enabled) {
    enabled_ = enabled;
}

void StepperMotor::setDirection(bool direction) {
    direction_ = direction;
}

void StepperMotor::setStalled(bool stalled) {
    stalled_ = stalled;
}

void StepperMotor::setPosition(std::int64_t position) {
    position_ = position;
}

void StepperMotor::setStepLevel(bool level) {
    // The spooler firmware enables TMC2130 CHOPCONF.DEDGE.  Therefore each
    // STEP-pin transition produces one physical microstep.
    if (level != stepLevel_ && enabled_ && !stalled_) {
        position_ += direction_ ? 1 : -1;
        ++stepCount_;
    }
    stepLevel_ = level;
}
