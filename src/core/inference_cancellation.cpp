// ============================================================================
// inference_cancellation.cpp — Inference Cancellation Implementation
// ============================================================================
// Mostly header-only for performance; this file provides non-inline helpers
// and any required TU-level state.
// ============================================================================
#include "inference_cancellation.h"

#include <algorithm>
#include <chrono>

namespace rawrxd {

// Any non-inline helpers would go here.  Currently the entire implementation
// is header-inlined for zero-overhead cancellation checking.
// This TU exists to ensure the header compiles cleanly and to anchor vtables
// if virtual methods are ever added.

// Intentionally minimal — the hot path is in the header.

} // namespace rawrxd
