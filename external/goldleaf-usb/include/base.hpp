#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cstddef>
#include <new>
#include <switch.h>

inline constexpr size_t operator""_KB(unsigned long long n) { return n * 0x400; }
inline constexpr size_t operator""_MB(unsigned long long n) { return operator""_KB(n) * 0x400; }
inline constexpr size_t operator""_GB(unsigned long long n) { return operator""_MB(n) * 0x400; }

namespace rc {
    static constexpr Result ResultSuccess = 0;
}

#ifndef GLEAF_ASSERT_FAIL
  #define GLEAF_ASSERT_FAIL(expr) diagAbortWithResult(MAKERESULT(354, 1))
#endif

#ifndef GLEAF_VERSION
  #ifdef APP_VERSION
    #define GLEAF_VERSION APP_VERSION
  #else
    #define GLEAF_VERSION "unknown"
  #endif
#endif
