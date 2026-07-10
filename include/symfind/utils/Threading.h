#pragma once

#include <thread>
#include <vector>

#include <sys/cdefs.h>

#include <symfind/utils/HWUtils.h>

#if 1
#define HARDWARE_CONCURRENCY_COUNT SymFind::get_hardware_concurrency()
#else // Debug
#define HARDWARE_CONCURRENCY_COUNT 1
#endif

namespace SymFind
{

using Thread = std::jthread;
using ThreadList = std::vector<Thread>;

namespace
{

__always_inline std::uint16_t get_proper_thread_count_to_process_list(std::size_t list_size)
{
    static const std::uint16_t THREADS_COUNT = HARDWARE_CONCURRENCY_COUNT;
    static const std::uint32_t MINIMUM_FILES_PER_THREAD = 20;

    auto CURRENT_FILES_PER_THREAD = static_cast<std::uint32_t>(list_size / THREADS_COUNT);

    if (CURRENT_FILES_PER_THREAD >= MINIMUM_FILES_PER_THREAD)
    {
        return THREADS_COUNT;
    }
    return std::max<std::uint16_t>(1, static_cast<std::uint16_t>(list_size / MINIMUM_FILES_PER_THREAD));
}

} // namespace

} // namespace SymFind
