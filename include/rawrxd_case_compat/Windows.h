// Prepended via CMake (WIN32 && !MSVC) only. Maps <Windows.h> to the next windows.h.
#pragma once
#if defined(__GNUC__) || defined(__clang__)
#include_next <windows.h>
#else
#include <windows.h>
#endif
