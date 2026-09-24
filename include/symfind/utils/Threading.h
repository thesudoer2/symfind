#pragma once

#include <thread>
#include <vector>

#include <sys/cdefs.h>

#include <symfind/filesystem/FileWrapper.h>
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

std::vector<FileIDList> compute_file_id_chunks(std::size_t N, std::size_t T) noexcept
{
    std::vector<FileIDList> chunks;
    chunks.reserve(T);

    std::size_t base = N / T;
    std::size_t remainder = N % T;

    std::uint32_t begin_index{0};
    std::uint32_t end_index{0};

    for (std::size_t i{0}; i < T; ++i)
    {
        std::size_t chunk_size = base + (i < remainder ? 1 : 0);
        end_index = begin_index + static_cast<std::uint32_t>(chunk_size);

        FileIDList file_ids;
        file_ids.reserve(chunk_size);
        for (std::uint32_t j = begin_index; j < end_index; ++j)
        {
            file_ids.push_back(j);
        }

        chunks.push_back(std::move(file_ids));
        begin_index = end_index;
    }

    return chunks;
}

template <typename FileList, typename W, typename... WorkerArgs>
void launch_threads_over_files(ThreadList &thread_list, const FileList &found_files, W worker, WorkerArgs &&...worker_args)
{
    auto copied_args = std::make_tuple(std::forward<WorkerArgs>(worker_args)...);

    const std::size_t N = found_files.size();
    const std::size_t T = thread_list.capacity();

    if (T == 0)
    {
        std::cerr << "Thread list is not reserved!\n";
        exit(EXIT_FAILURE);
    }

    std::vector<FileIDList> chunk_list = compute_file_id_chunks(N, T);
    for (FileIDList& chunk : chunk_list)
    {
        std::apply(
            [&](const auto &...copied_args) {
                thread_list.emplace_back(Thread(worker, std::move(chunk), std::ref(found_files), copied_args...));
            },
            copied_args);
    }
}

} // namespace

} // namespace SymFind
