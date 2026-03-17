#pragma once
// RawrXD Build Detection Header — Auto-generated compile-time metadata

#ifndef RAWR_BUILD_DETECT_HPP
#define RAWR_BUILD_DETECT_HPP

// Build timestamp injected at compile time
#ifndef RAWR_BUILD_TIMESTAMP
#define RAWR_BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

// Compiler detection
#if defined(_MSC_VER)
    #define RAWR_COMPILER "MSVC"
    #define RAWR_COMPILER_VER _MSC_VER
#elif defined(__clang__)
    #define RAWR_COMPILER "Clang"
    #define RAWR_COMPILER_VER __clang_major__
#elif defined(__GNUC__)
    #define RAWR_COMPILER "GCC"
    #define RAWR_COMPILER_VER __GNUC__
#else
    #define RAWR_COMPILER "Unknown"
    #define RAWR_COMPILER_VER 0
#endif

// Architecture detection
#if defined(_M_X64) || defined(__x86_64__)
    #define RAWR_ARCH "x64"
#elif defined(_M_ARM64)
    #define RAWR_ARCH "ARM64"
#else
    #define RAWR_ARCH "x86"
#endif

// Build config detection
#if defined(NDEBUG) || defined(RELEASE)
    #define RAWR_BUILD_CONFIG "Release"
#else
    #define RAWR_BUILD_CONFIG "Debug"
#endif

#define RAWR_BUILD_VERSION "14.2.0"

#endif // RAWR_BUILD_DETECT_HPP
