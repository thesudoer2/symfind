#pragma once

#include <cstdlib>
#include <iostream>
#include <tuple>

#include "FSScanner.h"
#include "Global.h"
#include "NoCopy.h"
#include "NoMove.h"
#include "Symbol.h"
#include "Threading.h"


// NOLINTBEGIN(cppcoreguidelines-special-member-functions,readability-identifier-length,readability-redundant-access-specifiers)

namespace SymFind
{

using IgnoreSymbolCallback = std::function<bool(const SymFind::SymbolEntry &)>;

class SymFinder final : NoCopy, NoMove
{
public:
    explicit SymFinder(ConfigParserPtr conf, const FSScanner &fsscanner, IgnoreSymbolCallback ignore_symbol) noexcept;

private:
    template <typename W, typename... WorkerArgs>
    void launch_threads(ThreadList &thread_list, W worker, WorkerArgs &&...worker_args);

private:
    ConfigParserPtr _conf;
    const FSScanner &_fsscanner;
};

// TODO: This function has lots of duplication with DatabaseBuilder::launch_threads function. Separate file-id distribution logic and use in both.
template <typename W, typename... WorkerArgs>
void SymFinder::launch_threads(ThreadList &thread_list, W worker, WorkerArgs &&...worker_args)
{
    auto copied_args = std::make_tuple(std::forward<WorkerArgs>(worker_args)...);

    const FSScanner::FileList &found_files = _fsscanner.get_found_files();

    const std::size_t N = found_files.size();
    const std::size_t T = thread_list.capacity();

    if (T == 0)
    {
        std::cerr << "Thread list is not reserved!\n";
        exit(EXIT_FAILURE);
    }

    std::size_t base = N / T;
    std::size_t remainder = N % T;

    std::uint32_t begin_index{0};
    std::uint32_t end_index{0};

    for (std::size_t i{0}; i < T; ++i)
    {
        std::size_t chunk_size = base + (i < remainder ? 1 : 0);

        end_index = begin_index + chunk_size;

        FileIDList file_ids;
        file_ids.reserve(chunk_size);
        for (std::uint32_t j = begin_index; j < end_index; ++j)
        {
            file_ids.push_back(j);
        }

        // TODO: Use a better parameter or config to determine verbosity!
        std::apply(
            [&](const auto &...copied_args) {
                thread_list.emplace_back(Thread(worker,
                                                std::move(file_ids),
                                                std::ref(found_files),
                                                copied_args...,
                                                this->_conf->get_debug_pruning()));
            },
            copied_args);

        begin_index = end_index;
    }
}

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-special-member-functions,readability-identifier-length,readability-redundant-access-specifiers)
