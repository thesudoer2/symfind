#pragma once

#include <utility>

namespace FindSymbol
{

template <typename T>
class Singleton
{
public:
    template <typename... Args>
    static void init(Args &&...args)
    {
        static bool initialized = false;
        if (!initialized)
        {
            instance_ptr() = new T(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-owning-memory)
            initialized = true;
        }
    }

    static T &getInstance()
    {
        return *instance_ptr();
    }

private:
    static T *&instance_ptr()
    {
        static T *ptr = nullptr;
        return ptr;
    }
};


} // namespace FindSymbol
