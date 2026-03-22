#pragma once

#include <string>

// E01 — Single inference façade (Win32)
//
// All user-driven chat / completion paths that perform model I/O must converge on:
//   Win32IDE::routeInferenceRequest / routeInferenceRequestAsync
// which dispatch through the BackendSwitcher (see docs/INFERENCE_PATH_MATRIX.md).
//
// Do not add parallel “shadow” inference entry points that bypass error prefixing
// (`[BackendSwitcher] Error`) and latency accounting on m_backendStatuses.

namespace rawrxd::ide {

inline constexpr const char* inferenceFacadeLogChannel() { return "InferenceFacade"; }

/// Human-readable lane name for structured logs (mirrors active AIBackendType).
inline std::string inferenceFacadeLaneField(const std::string& activeBackendName)
{
    return std::string("inference_lane=") + activeBackendName;
}

}  // namespace rawrxd::ide
