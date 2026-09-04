#include <symfind/utils/Timer.h>

namespace SymFind
{

void Timer::start() noexcept
{
    _start = Clock::now();
}

void Timer::stop() noexcept
{
    _stop = Clock::now();
    _stopped = true;
}

std::string Timer::duration() const noexcept
{
    const auto end = _stopped ? _stop : Clock::now();
    const auto elapsed = end - _start;

    const auto us =
        std::chrono::duration<double, std::micro>(elapsed).count();

    if (us < 1'000.0)
        return std::format("{:.2f} µs", us);

    const auto ms = us / 1'000.0;

    if (ms < 1'000.0)
        return std::format("{:.2f} ms", ms);

    const auto seconds = ms / 1'000.0;

    if (seconds < 60.0)
        return std::format("{:.2f} s", seconds);

    const auto minutes = seconds / 60.0;

    if (minutes < 60.0)
        return std::format("{:.2f} min", minutes);

    const auto hours = minutes / 60.0;

    return std::format("{:.2f} h", hours);
}

} // namespace SymFind
