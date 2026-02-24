#pragma once

#include <cstdio>

#ifdef NJCX_ENABLE_DEBUG_LOG
#define NJCX_DBG_LOG(...) std::fprintf(stderr, __VA_ARGS__)
#define NJCX_DBG_CODE(code) \
    do {                    \
        code;               \
    } while (0)
#else
#define NJCX_DBG_LOG(...) \
    do {                  \
    } while (0)
#define NJCX_DBG_CODE(code) \
    do {                    \
    } while (0)
#endif

