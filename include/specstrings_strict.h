#pragma once

// Local compatibility shim.
// This repository's include path precedes WinSDK include paths, so if WinSDK
// headers request <specstrings_strict.h>, they may resolve here first.
// Keep this file non-destructive so SAL annotations remain usable.

#ifndef _Return_type_success_
#define _Return_type_success_(expr)
#endif

#ifndef _Success_
#define _Success_(expr)
#endif

#ifndef _Check_return_
#define _Check_return_
#endif