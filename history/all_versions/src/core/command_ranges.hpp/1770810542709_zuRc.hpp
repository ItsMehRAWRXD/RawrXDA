// ============================================================================
// command_ranges.hpp — Category ID Range Constraints (Compile-Time)
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Defines valid ID ranges per command category.
// Used by command_registry.hpp for static_assert validation.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_COMMAND_RANGES_HPP
#define RAWRXD_COMMAND_RANGES_HPP

#include <cstdint>

namespace RawrXD::Ranges {

// ============================================================================
// CATEGORY RANGE TABLE
// Each category owns a contiguous ID range.
// Adding an IDM_* outside its category range → compile error.
// ============================================================================

// File Operations  (from Win32IDE_Commands.cpp range 1000-1999)
constexpr uint32_t FILE_MIN   = 1001;
constexpr uint32_t FILE_MAX   = 1099;

// Edit Operations  (range 2000-2099)
constexpr uint32_t EDIT_MIN   = 2001;
constexpr uint32_t EDIT_MAX   = 2099;

// View Operations  (range 2020-2029 in Win32IDE.cpp, also 3000-3099)
constexpr uint32_t VIEW_MIN   = 2020;
constexpr uint32_t VIEW_MAX   = 2029;

// Git              (range 3020-3024)
constexpr uint32_t GIT_MIN    = 3020;
constexpr uint32_t GIT_MAX    = 3029;

// Theme            (range 3100-3117)
constexpr uint32_t THEME_MIN  = 3100;
constexpr uint32_t THEME_MAX  = 3117;

// Transparency     (range 3200-3211)
constexpr uint32_t TRANS_MIN  = 3200;
constexpr uint32_t TRANS_MAX  = 3211;

// Help             (range 4001-4004 legacy, 7001-7003 non-colliding)
constexpr uint32_t HELP_LEGACY_MIN = 4001;
constexpr uint32_t HELP_LEGACY_MAX = 4004;
constexpr uint32_t HELP_MIN   = 7001;
constexpr uint32_t HELP_MAX   = 7003;

// Terminal         (range 4001-4010, overlaps with Help legacy)
constexpr uint32_t TERM_MIN   = 4001;
constexpr uint32_t TERM_MAX   = 4010;

// Agent            (range 4100-4121)
constexpr uint32_t AGENT_MIN  = 4100;
constexpr uint32_t AGENT_MAX  = 4121;

// SubAgent         (range 4110-4114, subset of Agent)
constexpr uint32_t SUBAGENT_MIN = 4110;
constexpr uint32_t SUBAGENT_MAX = 4114;

// Autonomy         (range 4150-4155)
constexpr uint32_t AUTO_MIN   = 4150;
constexpr uint32_t AUTO_MAX   = 4155;

// AI Mode          (range 4200-4203)
constexpr uint32_t AIMODE_MIN = 4200;
constexpr uint32_t AIMODE_MAX = 4203;

// AI Context       (range 4210-4216)
constexpr uint32_t AICTX_MIN  = 4210;
constexpr uint32_t AICTX_MAX  = 4216;

// Reverse Engineering (range 4300-4319)
constexpr uint32_t REVENG_MIN = 4300;
constexpr uint32_t REVENG_MAX = 4319;

// Backend Switcher (range 5037-5047)
constexpr uint32_t BACKEND_MIN = 5037;
constexpr uint32_t BACKEND_MAX = 5047;

// Router           (range 5048-5081)
constexpr uint32_t ROUTER_MIN = 5048;
constexpr uint32_t ROUTER_MAX = 5081;

// LSP Client       (range 5058-5070)
constexpr uint32_t LSP_MIN    = 5058;
constexpr uint32_t LSP_MAX    = 5070;

// ASM Semantic     (range 5082-5093)
constexpr uint32_t ASM_MIN    = 5082;
constexpr uint32_t ASM_MAX    = 5093;

// Hybrid LSP-AI    (range 5094-5105)
constexpr uint32_t HYBRID_MIN = 5094;
constexpr uint32_t HYBRID_MAX = 5105;

// Multi-Response   (range 5106-5117)
constexpr uint32_t MULTI_MIN  = 5106;
constexpr uint32_t MULTI_MAX  = 5117;

// Governor         (range 5118-5121)
constexpr uint32_t GOV_MIN    = 5118;
constexpr uint32_t GOV_MAX    = 5121;

// Safety           (range 5122-5125)
constexpr uint32_t SAFETY_MIN = 5122;
constexpr uint32_t SAFETY_MAX = 5125;

// Replay           (range 5126-5129)
constexpr uint32_t REPLAY_MIN = 5126;
constexpr uint32_t REPLAY_MAX = 5129;

// Confidence       (range 5130-5131)
constexpr uint32_t CONF_MIN   = 5130;
constexpr uint32_t CONF_MAX   = 5131;

// Swarm            (range 5132-5156)
constexpr uint32_t SWARM_MIN  = 5132;
constexpr uint32_t SWARM_MAX  = 5156;

// Debug/DBG        (range 5157-5184)
constexpr uint32_t DBG_MIN    = 5157;
constexpr uint32_t DBG_MAX    = 5184;

// Plugin           (range 5200-5208)
constexpr uint32_t PLUGIN_MIN = 5200;
constexpr uint32_t PLUGIN_MAX = 5208;

// Hotpatch         (range 9001-9017)
constexpr uint32_t HOTPATCH_MIN = 9001;
constexpr uint32_t HOTPATCH_MAX = 9017;

// Monaco/View      (range 9100-9105)
constexpr uint32_t MONACO_MIN = 9100;
constexpr uint32_t MONACO_MAX = 9105;

// LSP Server       (range 9200-9208)
constexpr uint32_t LSPSRV_MIN = 9200;
constexpr uint32_t LSPSRV_MAX = 9208;

// Editor Engine    (range 9300-9304)
constexpr uint32_t EDITOR_MIN = 9300;
constexpr uint32_t EDITOR_MAX = 9304;

// PDB              (range 9400-9412)
constexpr uint32_t PDB_MIN    = 9400;
constexpr uint32_t PDB_MAX    = 9412;

// Audit            (range 9500-9506)
constexpr uint32_t AUDIT_MIN  = 9500;
constexpr uint32_t AUDIT_MAX  = 9506;

// Gauntlet         (range 9600-9601)
constexpr uint32_t GAUNTLET_MIN = 9600;
constexpr uint32_t GAUNTLET_MAX = 9601;

// Voice            (range 9700-9709)
constexpr uint32_t VOICE_MIN  = 9700;
constexpr uint32_t VOICE_MAX  = 9709;

// QW (Quality/Workflow) (range 9800-9830)
constexpr uint32_t QW_MIN     = 9800;
constexpr uint32_t QW_MAX     = 9830;

// Telemetry        (range 9900-9905)
constexpr uint32_t TELEM_MIN  = 9900;
constexpr uint32_t TELEM_MAX  = 9905;

// CLI-Only         (ID = 0 means no GUI mapping)
constexpr uint32_t CLI_ONLY   = 0;

} // namespace RawrXD::Ranges

#endif // RAWRXD_COMMAND_RANGES_HPP
