#pragma once

#include <windows.h>
#include <cstddef>

#include "Win32IDE.h"

namespace RawrXD {

static constexpr unsigned long long RAWRXD_SOVEREIGN_TELEMETRY_WPARAM = RAWRXD_SOVEREIGN_HARNESS_SENTINEL;

void setStreamingBeaconUiNotifyWindow(HWND hwnd);
HWND getStreamingBeaconUiNotifyWindow();
void postSovereignStreamStart();
void postSovereignStreamToken(const char* token, size_t length = 0);
void postSovereignStreamSuccess(double tps, int totalTokens, const char* schemaType);

} // namespace RawrXD
