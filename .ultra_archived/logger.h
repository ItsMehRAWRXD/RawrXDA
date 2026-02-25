// Production Logger - redirects to include/logging/logger.h
// Full implementation with file output, log levels, timestamps, mutex safety,
// and variadic {} format placeholders.
//
// Migration: old code using Logger("prefix").info("msg") still works.
// New code should use Logger("name", "logs/").info("format {}", arg).
#include "include/logging/logger.h"
