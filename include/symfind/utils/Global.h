#ifndef __always_inline
#define __always_inline inline __attribute__((__always_inline__))
#endif

#ifndef __static_always_inline
#define __static_always_inline static __always_inline
#endif

#ifndef __nodiscard
#define __nodiscard [[nodiscard]]
#define __nodiscard_msg(msg) [[nodiscard(msg)]]
#endif

#ifndef SYMFIND_PACK
#if defined(_MSC_VER) // Microsoft MSVC
#define SYMFIND_PACK(...) __pragma(pack(push, 1)) __VA_ARGS__ __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__) // GCC/CLANG
#define SYMFIND_PACK(...) __VA_ARGS__ __attribute__((packed))
#else
#define SYMFIND_PACK(...) __VA_ARGS__
#endif
#endif

#define SET_ERR_MSG(err_msg_buf, err_msg)                                                                              \
    {                                                                                                                  \
        if ((err_msg_buf) != nullptr)                                                                                  \
            *(err_msg_buf) = err_msg;                                                                                  \
    }

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
