#include "ShiftRegister.hh"
#include "MmuBoard.hh"

ShiftRegister16::ShiftRegister16(MmuBoard &board)
    : board_(board), data_(false), clock_(false), latch_(false), shifted_(0), latched_(0) {}

void ShiftRegister16::setData(bool value) {
    data_ = value;
}

void ShiftRegister16::setClock(bool value) {
    // 74HC595 shifts on the rising edge.  The firmware sends bit 15 first,
    // therefore after 16 clocks the natural shift operation produces the
    // same 16-bit value used by hal::shr16::SHR16::Write().
    if (!clock_ && value) {
        shifted_ = static_cast<std::uint16_t>((shifted_ << 1) | (data_ ? 1U : 0U));
    }
    clock_ = value;
}

void ShiftRegister16::setLatch(bool value) {
    if (!latch_ && value) {
        latched_ = shifted_;
        board_.shiftRegisterLatched(latched_);
    }
    latch_ = value;
}
