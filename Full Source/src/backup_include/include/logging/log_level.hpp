#pragma once

// windows.h may define ERROR as a macro; avoid enum token corruption.
#ifdef ERROR
#undef ERROR
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef WARN
#undef WARN
#endif
#ifdef CRITICAL
#undef CRITICAL
#endif

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRITICAL = 4
};
