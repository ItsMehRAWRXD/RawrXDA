// Canonical declaration lives in src/agentic_observability.h.
// Keep this public include as a forwarding shim so all translation units use
// one class layout and avoid ODR/ABI mismatches in debug/runtime.
#pragma once

#include "../src/agentic_observability.h"
