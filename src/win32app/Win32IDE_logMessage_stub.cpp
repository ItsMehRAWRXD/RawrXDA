// Build-compat shim for legacy logMessage stub references.
// The production implementation lives in src/win32app/Win32IDE_logMessage.cpp.

namespace {
static unsigned g_win32IdeLogMessageStubHits = 0;
}

extern "C" void RawrXD_Win32IDELogMessageStubAnchor() {
    g_win32IdeLogMessageStubHits += 1;
}
