// Stub for ErrorReporter
#include "ErrorReporter.hpp"
#include <iostream>

void ErrorReporter::report(const std::string& error) {
    std::cerr << error << std::endl;
}

void ErrorReporter::report(const std::string& error, HWND owner) {
    (void)owner;
    std::cerr << error << std::endl;
}