#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winuser.h>
#include <commctrl.h>
#include <richedit.h>
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <locale>


// Small helpers
inline void safe_log(const std::string &msg) {
    std::cerr << msg << std::endl;
}
