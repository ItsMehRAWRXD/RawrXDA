#pragma once

#include <string>

// Forward declare HWND to avoid including windows.h in header
#ifndef _WINDEF_
struct HWND__;
typedef struct HWND__* HWND;
#endif

/**
 * @brief Simple error reporting helper.
 *
 * Writes the error message to a log file (RawrXD_error.log in the executable
 * directory) and shows a modal MessageBox to the user. All error handling in
 * the IDE should funnel through this class to keep the logic consistent.
 */
class ErrorReporter {
public:
    /**
     * Report an error message.
     * @param msg    Human‑readable error description.
     * @param parent Optional HWND of the parent window for the MessageBox.
     */
    static void report(const std::string& msg, HWND parent = nullptr);
};
