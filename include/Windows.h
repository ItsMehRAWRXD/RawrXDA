// Case-compat shim for cross-platform MinGW builds on case-sensitive filesystems.
// Many upstream units include <Windows.h> (MSVC case); MinGW provides <windows.h>.
#pragma once
#include <windows.h>
