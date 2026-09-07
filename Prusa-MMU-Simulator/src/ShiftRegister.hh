#pragma once

#include <cstdint>

class MmuBoard;

class ShiftRegister16 {
public:
    explicit ShiftRegister16(MmuBoard &board);

    void setData(bool value);
    void setClock(bool value);
    void setLatch(bool value);

    std::uint16_t value() const { return latched_; }

private:
    MmuBoard &board_;
    bool data_;
    bool clock_;
    bool latch_;
    std::uint16_t shifted_;
    std::uint16_t latched_;
};
