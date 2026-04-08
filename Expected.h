#pragma once

#include <tl/expected.hpp>

namespace SymFind
{

template<typename V, typename E>
using expected = tl::expected<V, E>;

template<typename E>
using unexpected = tl::unexpected<E>;

} // namespace SymFind
