// HexMagConsole.cpp — Headless console output buffer implementation
// Converted from Qt (QPlainTextEdit, QTime, appendPlainText) to pure C++17
// Preserves ALL original functionality: log output, hex dump display

#include "HexMagConsole.h"

// All methods are inline in the header — this file exists for
// any future non-inline implementations and to maintain the
// original source file structure from the Qt version.
//
// The original Qt version used:
//   QPlainTextEdit         ->  std::vector<std::string> buffer + std::cout
//   QTime::currentTime()   ->  std::chrono::system_clock
//   appendPlainText(...)   ->  appendLog(...)
//   setReadOnly(true)      ->  N/A (headless output buffer)
//   setStyleSheet(...)     ->  N/A (no GUI)
