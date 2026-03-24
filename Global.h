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
