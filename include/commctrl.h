#pragma once

// Wrapper to stabilize SAL/CALLBACK macro availability before including
// the WinSDK common controls header. The project include path precedes
// the SDK include path, so this file centralizes the compatibility fix.
#ifndef _Return_type_success_
#define _Return_type_success_(expr)
#endif

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

#include "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um/commctrl.h"
