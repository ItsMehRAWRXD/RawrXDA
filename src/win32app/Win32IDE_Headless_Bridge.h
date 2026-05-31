#pragma once

// ============================================================================
// Win32IDE_Headless_Bridge.h - Minimal Bridge for Headless Builds
// ============================================================================
//
// This header provides the absolute minimum declarations required for modules
// like `vscode_extension_api` to link against the `RawrXD_Gold` target without
// pulling in the entire GUI-heavy `Win32IDE.h`.
//
// It declares the `Win32IDE` class and the specific functions that are
// implemented in a headless-compatible way (e.g., in Win32IDE_logMessage.cpp).
//
// ============================================================================

#include <string>
#include <vector>

// Forward declare the main IDE class
class Win32IDE;

// Declare only the functions that are safe to call in a headless context
// and are required by the including source files.
namespace vscode {
    void showInformationMessage(const std::string& message);
    void showErrorMessage(const std::string& message);
}

// Provide a minimal logging function declaration that is implemented elsewhere
// for headless builds.
void logMessage(const char* format, ...);
