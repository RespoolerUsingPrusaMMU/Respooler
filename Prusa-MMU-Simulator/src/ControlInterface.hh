#pragma once

#include <cstdint>
#include <string>

class MmuBoard;

class ControlInterface {
public:
    explicit ControlInterface(const std::string &fifoPath);
    ~ControlInterface();

    bool initialize();
    void poll(MmuBoard &board);
    const std::string &path() const { return fifoPath_; }

private:
    enum class TapState : std::uint8_t {
        Idle,
        PreRelease,
        Pressed,
        PostRelease
    };

    void executeLine(MmuBoard &board, const std::string &line);
    void startButtonTap(MmuBoard &board,
                        const std::string &buttonName,
                        std::uint32_t holdMilliseconds);
    void updateButtonTap(MmuBoard &board);
    void cancelButtonTap(MmuBoard &board);

    static std::uint64_t millisecondsToCycles(const MmuBoard &board,
                                               std::uint32_t milliseconds);
    static const char *buttonName(int buttonValue);

    std::string fifoPath_;
    int fd_;
    std::string pending_;

    TapState tapState_;
    int tapButtonValue_;
    std::uint32_t tapHoldMilliseconds_;
    std::uint64_t tapPhaseEndCycle_;
};
