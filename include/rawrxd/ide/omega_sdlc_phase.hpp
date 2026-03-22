#pragma once

#include <cstdint>
#include <string>

namespace rawrxd::ide {

/// E04 — Explicit SDLC phases for long-running agent/plan work (structured logs + UI hooks).
enum class OmegaSdlcPhase : std::uint8_t
{
    Plan = 1,
    Mutate = 2,
    Verify = 3,
    Ship = 4
};

inline const char* omegaPhaseToken(OmegaSdlcPhase p)
{
    switch (p)
    {
    case OmegaSdlcPhase::Plan: return "plan";
    case OmegaSdlcPhase::Mutate: return "mutate";
    case OmegaSdlcPhase::Verify: return "verify";
    case OmegaSdlcPhase::Ship: return "ship";
    default: return "unknown";
    }
}

/// Single-line field bundle for grep-friendly observability (correlates with PR02-style runbooks).
inline std::string omegaStructuredLog(OmegaSdlcPhase p, const char* detail)
{
    std::string s = "omega_phase=";
    s += omegaPhaseToken(p);
    s += " omega_detail=";
    s += (detail && detail[0]) ? detail : "(none)";
    return s;
}

}  // namespace rawrxd::ide
