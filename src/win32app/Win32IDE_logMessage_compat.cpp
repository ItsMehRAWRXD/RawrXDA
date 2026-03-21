// Build-compat shim for legacy logMessage wiring.
// The production implementation lives in src/win32app/Win32IDE_logMessage.cpp.

extern "C" void RawrXD_Win32IDELogMessageStubAnchor() {}
