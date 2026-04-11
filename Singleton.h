#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace SymFind
{

template <typename T>
class Singleton
{
public:
    using InstancePtr = std::shared_ptr<T>;

    /// Construct instance internally (can be called only once)
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

    /// Get the constructed instance as reference
    static T &getInstance()
    {
        if (!instance_ptr())
        {
            throw std::runtime_error("Singleton not initialized");
        }

        return *instance_ptr();
    }

    /// Get the instance as InstancePtr
    /// This is the new function you requested
    static InstancePtr getInstancePtr()
    {
        std::lock_guard<std::mutex> lock(mutex());

        if (!instance_ptr())
        {
            throw std::runtime_error("Singleton not initialized");
        }

        // Create a shared_ptr that does NOT take ownership (the raw pointer is still managed by the Singleton)
        return InstancePtr(instance_ptr(), [](T *) { /* do nothing - Singleton owns the lifetime */ });
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
