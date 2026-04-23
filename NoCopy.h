#pragma once

// NOLINTBEGIN(cppcoreguidelines-special-member-functions)

namespace SymFind
{

/// Base class for types that should not be copy-assigned.
class NoCopyAssign
{
public:
    NoCopyAssign& operator=(const NoCopyAssign &) = delete;
    NoCopyAssign(const NoCopyAssign &) = default;
    NoCopyAssign() = default;
};

/// Base class for types that should not be copied or assigned
class NoCopy : NoCopyAssign
{
public:
    NoCopy(NoCopy &&) = delete;
    NoCopy() = default;
};

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-special-member-functions)
