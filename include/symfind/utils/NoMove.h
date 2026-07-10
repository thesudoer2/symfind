#pragma once

// NOLINTBEGIN(cppcoreguidelines-special-member-functions)

namespace SymFind
{

/// Base class for types that should not be move-assigned.
class NoMoveAssign
{
public:
    NoMoveAssign& operator=(NoMoveAssign &&) = delete;
    NoMoveAssign(NoMoveAssign &&) = default;
    NoMoveAssign() = default;
};

/// Base class for types that should not be moved
class NoMove : NoMoveAssign
{
public:
    NoMove(NoMove &&) = delete;
    NoMove() = default;
};

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-special-member-functions)
