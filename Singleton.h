#pragma once

#include <mutex>
#include <stdexcept>
#include <utility>

namespace SymFind
{

template <typename T>
class Singleton
{
public:
    /// Construct instance internally
    template <typename... Args>
    static void init(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(mutex());

        static bool initialized = false;
        if (!initialized)
        {
            instance_ptr() = new T(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-owning-memory)
            initialized = true;
        }
    }

    /// Get the constructed instance
    static T &getInstance()
    {
        if (!instance_ptr())
        {
            throw std::runtime_error("Singleton not initialized");
        }

        return *instance_ptr();
    }

private:
    static std::mutex &mutex()
    {
        static std::mutex mut;
        return mut;
    }

    static T *&instance_ptr()
    {
        static T *ptr = nullptr;
        return ptr;
    }
};

} // namespace SymFind
