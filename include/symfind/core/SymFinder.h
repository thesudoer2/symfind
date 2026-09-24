#pragma once

#include <cstdlib>
#include <iostream>
#include <tuple>

#include <symfind/core/Symbol.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/utils/Global.h>
#include <symfind/utils/NoCopy.h>
#include <symfind/utils/NoMove.h>
#include <symfind/utils/Threading.h>

// NOLINTBEGIN(cppcoreguidelines-special-member-functions,readability-identifier-length,readability-redundant-access-specifiers)

namespace SymFind
{

using IgnoreSymbolCallback = std::function<bool(const SymFind::SymbolEntry &)>;

class SymFinder : NoCopy, NoMove
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
    launch_threads_over_files(thread_list,
                               _fsscanner.get_found_files(),
                               worker,
                               std::forward<WorkerArgs>(worker_args)...,
                               this->_conf->get_debug_pruning());
}

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-special-member-functions,readability-identifier-length,readability-redundant-access-specifiers)
