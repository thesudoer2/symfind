#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <string>

namespace SymFind
{

class Timer
{
public:
    using Clock = std::chrono::steady_clock;

    void start() noexcept;

    void stop() noexcept;

    [[nodiscard]]
    std::string duration() const noexcept;

private:
    Clock::time_point _start;
    Clock::time_point _stop;

    bool _stopped{false};
};

} // namespace SymFind
