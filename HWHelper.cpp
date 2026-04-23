#include "HWHelper.h"

#include <thread>

namespace SymFind
{

std::uint16_t get_hardware_concurrency() noexcept
{
    return std::thread::hardware_concurrency();
}

} // namespace SymFind
