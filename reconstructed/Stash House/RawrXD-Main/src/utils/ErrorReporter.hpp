#pragma once

#include <string>
#include <windows.h>

class ErrorReporter {
public:
    static void report(const std::string& error);
    static void report(const std::string& error, HWND owner);
};
