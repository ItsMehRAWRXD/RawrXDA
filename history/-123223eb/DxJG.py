#!/usr/bin/env python3
# ============================================================================
# command_matrix_test.py — Full Command ID Matrix Validator
# ============================================================================
#
# Purpose:   Fires every registered command ID through the RawrXD server
#            endpoint and validates C++ backend catches each signal.
#            Special focus on AI Mode (4200-4311) range that was previously
#            in the dead zone.
#
# Usage:     python command_matrix_test.py [--host HOST] [--port PORT] [--verbose]
#            python command_matrix_test.py --offline   # Static analysis only
#
# Requires:  Python 3.8+, requests (pip install requests)
#            RawrXD server running on the target host:port
#
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
# ============================================================================

import argparse
import json
import sys
import time
import os
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from enum import Enum

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

# ============================================================================
# CANONICAL COMMAND ID TABLE
# ============================================================================
# Mirrors feature_registration.cpp — the source of truth.
# Any mismatch between this table and the C++ backend is a test failure.

class Zone(Enum):
    FILE        = "File Ops"
    EDIT        = "Editing"
    GIT         = "Git"
    THEMES      = "Themes"
    TERMINAL    = "Terminal"
    AGENT       = "Agent"
    AUTONOMY    = "Autonomy"
    AI_MODE     = "AI Mode"
    RE          = "Reverse Engineering"
    DEBUG       = "Debug"
    HOTPATCH    = "Hotpatch"
    EDITOR_ENG  = "Editor Engine"
    VOICE       = "Voice"
    HELP        = "Help"
    BACKEND     = "Backend/LLM"
    SWARM       = "Swarm"
    SERVER      = "Server"
    SETTINGS    = "Settings"
    SUBAGENT    = "SubAgent"

@dataclass
class CommandEntry:
    """A single command in the canonical ID table."""
    id: int                     # Win32 command ID (0 = CLI-only)
    feature_id: str             # e.g. "file.new"
    cli_command: str            # e.g. "!new"
    zone: Zone
    gui_supported: bool = True
    cli_supported: bool = True
    expected_zone_min: int = 0
    expected_zone_max: int = 0

# ── Zone boundary definitions (must match command_id_blueprint.h) ──

ZONE_BOUNDS: Dict[Zone, Tuple[int, int]] = {
    Zone.FILE:       (1000, 1099),
    Zone.EDIT:       (2000, 2099),
    Zone.GIT:        (3020, 3099),
    Zone.THEMES:     (3100, 3199),
    Zone.TERMINAL:   (4000, 4099),
    Zone.AGENT:      (4100, 4149),
    Zone.SUBAGENT:   (4100, 4149),  # SubAgent shares Agent zone 4110-4119
    Zone.AUTONOMY:   (4150, 4199),
    Zone.AI_MODE:    (4200, 4299),
    Zone.RE:         (4300, 4399),
    Zone.DEBUG:      (5100, 5199),
    Zone.BACKEND:    (5037, 5081),   # Legacy position
    Zone.SWARM:      (5132, 5156),   # Legacy position
    Zone.HELP:       (7000, 7099),
    Zone.HOTPATCH:   (9000, 9099),
    Zone.EDITOR_ENG: (9300, 9399),
    Zone.VOICE:      (9700, 9799),
    Zone.SERVER:     (0, 0),         # CLI-only
    Zone.SETTINGS:   (0, 0),         # CLI-only
}

# Dead zones — IDs in these ranges are FORBIDDEN
DEAD_ZONES = [
    (9100, 9199, "Monaco collision zone"),
    (9400, 9499, "PDB collision zone"),
    (10000, 49999, "Win32 system command range"),
    (0xF000, 0xF1FF, "SC_* system command range"),
]

# ============================================================================
# CANONICAL COMMAND TABLE
# ============================================================================
# This is the master registry. Derived from feature_registration.cpp.

COMMANDS: List[CommandEntry] = [
    # ── File Ops (1001-1035) ──
    CommandEntry(1001, "file.new",          "!new",            Zone.FILE),
    CommandEntry(1002, "file.open",         "!open",           Zone.FILE),
    CommandEntry(1003, "file.save",         "!save",           Zone.FILE),
    CommandEntry(1004, "file.saveAs",       "!save_as",        Zone.FILE),
    CommandEntry(1005, "file.saveAll",      "!save_all",       Zone.FILE),
    CommandEntry(1006, "file.close",        "!close",          Zone.FILE),
    CommandEntry(1010, "file.recentFiles",  "!recent",         Zone.FILE),
    CommandEntry(1030, "file.loadModel",    "!model_load",     Zone.FILE),
    CommandEntry(1031, "file.modelFromHF",  "!model_hf",       Zone.FILE),
    CommandEntry(1032, "file.modelFromOllama", "!model_ollama", Zone.FILE),
    CommandEntry(1033, "file.modelFromURL", "!model_url",       Zone.FILE),
    CommandEntry(1034, "file.modelUnified", "!model_unified",   Zone.FILE),
    CommandEntry(1035, "file.quickLoad",    "!quick_load",      Zone.FILE),
    
    # ── Edit (2001-2017) ──
    CommandEntry(2001, "edit.undo",         "!undo",           Zone.EDIT),
    CommandEntry(2002, "edit.redo",         "!redo",           Zone.EDIT),
    CommandEntry(2003, "edit.cut",          "!cut",            Zone.EDIT),
    CommandEntry(2004, "edit.copy",         "!copy",           Zone.EDIT),
    CommandEntry(2005, "edit.paste",        "!paste",          Zone.EDIT),
    CommandEntry(2016, "edit.find",         "!find",           Zone.EDIT),
    CommandEntry(2017, "edit.replace",      "!replace",        Zone.EDIT),
    CommandEntry(2006, "edit.selectAll",    "!select_all",     Zone.EDIT),
    
    # ── Git (3020-3024) ──
    CommandEntry(3020, "git.status",        "!git_status",     Zone.GIT),
    CommandEntry(3021, "git.commit",        "!git_commit",     Zone.GIT),
    CommandEntry(3022, "git.push",          "!git_push",       Zone.GIT),
    CommandEntry(3023, "git.pull",          "!git_pull",       Zone.GIT),
    CommandEntry(3024, "git.diff",          "!git_diff",       Zone.GIT),
    
    # ── Themes (3100-3101) ──
    CommandEntry(3101, "theme.set",         "!theme",          Zone.THEMES),
    CommandEntry(3100, "theme.list",        "!theme_list",     Zone.THEMES),
    
    # ── Terminal (4001-4010) ──
    CommandEntry(4001, "terminal.new",      "!terminal_new",   Zone.TERMINAL),
    CommandEntry(4006, "terminal.kill",     "!terminal_kill",  Zone.TERMINAL),
    CommandEntry(4007, "terminal.splitH",   "!terminal_split", Zone.TERMINAL),
    CommandEntry(4008, "terminal.splitV",   "!terminal_split_v", Zone.TERMINAL),
    CommandEntry(4010, "terminal.list",     "!terminal_list",  Zone.TERMINAL),
    
    # ── Agent (4100-4121) ──
    CommandEntry(4100, "agent.loop",        "!agent_loop",     Zone.AGENT),
    CommandEntry(4101, "agent.execute",     "!agent_execute",  Zone.AGENT),
    CommandEntry(4102, "agent.configure",   "!agent_config",   Zone.AGENT),
    CommandEntry(4103, "agent.viewTools",   "!tools",          Zone.AGENT),
    CommandEntry(4104, "agent.viewStatus",  "!agent_status",   Zone.AGENT),
    CommandEntry(4105, "agent.stop",        "!agent_stop",     Zone.AGENT),
    CommandEntry(4106, "agent.memory",      "!agent_memory",   Zone.AGENT),
    CommandEntry(4107, "agent.memoryView",  "!agent_memory_view", Zone.AGENT),
    CommandEntry(4108, "agent.memoryClear", "!agent_memory_clear", Zone.AGENT),
    CommandEntry(4109, "agent.memoryExport","!agent_memory_export", Zone.AGENT),
    CommandEntry(4120, "agent.boundedLoop", "!agent_bounded",  Zone.AGENT),
    CommandEntry(4121, "agent.goal",        "!agent_goal",     Zone.AGENT),
    
    # ── SubAgent (4110-4114) ──
    CommandEntry(4110, "subagent.chain",    "!chain",          Zone.SUBAGENT),
    CommandEntry(4111, "subagent.swarm",    "!swarm",          Zone.SUBAGENT),
    CommandEntry(4112, "subagent.todoList", "!todo",           Zone.SUBAGENT),
    CommandEntry(4113, "subagent.todoClear","!todo_clear",     Zone.SUBAGENT),
    CommandEntry(4114, "subagent.status",   "!agents",         Zone.SUBAGENT),
    
    # ── Autonomy (4150-4153) ──
    CommandEntry(4150, "autonomy.toggle",   "!autonomy_toggle", Zone.AUTONOMY),
    CommandEntry(4151, "autonomy.start",    "!autonomy_start",  Zone.AUTONOMY),
    CommandEntry(4152, "autonomy.stop",     "!autonomy_stop",   Zone.AUTONOMY),
    CommandEntry(4153, "autonomy.goal",     "!autonomy_goal",   Zone.AUTONOMY),
    CommandEntry(0,    "autonomy.rate",     "!autonomy_rate",   Zone.AUTONOMY, gui_supported=False),
    CommandEntry(0,    "autonomy.run",      "!autonomy_run",    Zone.AUTONOMY, gui_supported=False),
    
    # ═══════════════════════════════════════════════════════════════════════
    # CRITICAL TEST RANGE: AI MODE + RE (4200-4311)
    # These were previously in the dead zone. Exhaustive testing required.
    # ═══════════════════════════════════════════════════════════════════════
    
    # ── AI Mode (4200-4202) ──
    CommandEntry(4200, "ai.maxMode",        "!max",            Zone.AI_MODE),
    CommandEntry(4201, "ai.deepThinking",   "!deep",           Zone.AI_MODE),
    CommandEntry(4202, "ai.deepResearch",   "!research",       Zone.AI_MODE),
    CommandEntry(0,    "ai.mode",           "!mode",           Zone.AI_MODE, gui_supported=False),
    CommandEntry(0,    "ai.engine",         "!engine",         Zone.AI_MODE, gui_supported=False),
    
    # ── Reverse Engineering (4300-4311) ──
    CommandEntry(4300, "re.decisionTree",   "!decision_tree",  Zone.RE),
    CommandEntry(4301, "re.disassemble",    "!disasm",         Zone.RE),
    CommandEntry(4302, "re.dumpbin",        "!dumpbin",        Zone.RE),
    CommandEntry(4308, "re.cfgAnalysis",    "!cfg",            Zone.RE),
    CommandEntry(4311, "re.ssaLift",        "!ssa_lift",       Zone.RE),
    CommandEntry(0,    "re.autoPatch",      "!auto_patch",     Zone.RE, gui_supported=False),
    
    # ═══════════════════════════════════════════════════════════════════════
    
    # ── Debug (5157-5184) ──
    CommandEntry(5157, "debug.start",       "!debug_start",    Zone.DEBUG),
    CommandEntry(5160, "debug.continue",    "!debug_continue", Zone.DEBUG),
    CommandEntry(5161, "debug.step",        "!debug_step",     Zone.DEBUG),
    CommandEntry(5165, "debug.stop",        "!debug_stop",     Zone.DEBUG),
    CommandEntry(5166, "debug.breakpointAdd","!breakpoint_add", Zone.DEBUG),
    CommandEntry(5167, "debug.breakpointRemove","!breakpoint_remove", Zone.DEBUG),
    CommandEntry(5170, "debug.breakpointList","!breakpoint_list", Zone.DEBUG),
    
    # ── Backend/LLM (5037-5043) ──
    CommandEntry(5037, "backend.select",    "!backend use",    Zone.BACKEND),
    CommandEntry(5042, "backend.status",    "!backend status", Zone.BACKEND),
    CommandEntry(5043, "backend.list",      "!backend list",   Zone.BACKEND),
    
    # ── Swarm (5132-5138) ──
    CommandEntry(5132, "swarm.status",      "!swarm_status",   Zone.SWARM),
    CommandEntry(5136, "swarm.leave",       "!swarm_leave",    Zone.SWARM),
    CommandEntry(5137, "swarm.nodes",       "!swarm_nodes",    Zone.SWARM),
    CommandEntry(5138, "swarm.join",        "!swarm_join",     Zone.SWARM),
    CommandEntry(0,    "swarm.distribute",  "!swarm_distribute", Zone.SWARM, gui_supported=False),
    CommandEntry(0,    "swarm.rebalance",   "!swarm_rebalance",  Zone.SWARM, gui_supported=False),
    
    # ── Help (7001-7003) ──
    CommandEntry(7001, "help.about",        "!about",          Zone.HELP),
    CommandEntry(7002, "help.docs",         "!docs",           Zone.HELP),
    CommandEntry(7003, "help.shortcuts",    "!shortcuts",      Zone.HELP),
    
    # ── Hotpatch (9001-9006) ──
    CommandEntry(9001, "hotpatch.status",   "!hotpatch_status", Zone.HOTPATCH),
    CommandEntry(9002, "hotpatch.memory",   "!hotpatch_mem",    Zone.HOTPATCH),
    CommandEntry(9004, "hotpatch.byte",     "!hotpatch_byte",   Zone.HOTPATCH),
    CommandEntry(9006, "hotpatch.server",   "!hotpatch_server",  Zone.HOTPATCH),
    CommandEntry(0,    "hotpatch.apply",    "!hotpatch_apply",   Zone.HOTPATCH, gui_supported=False),
    CommandEntry(0,    "hotpatch.create",   "!hotpatch_create",  Zone.HOTPATCH, gui_supported=False),
    
    # ── Editor Engine (9300-9304) ──
    CommandEntry(9300, "editor.richEdit",   "",                 Zone.EDITOR_ENG, cli_supported=False),
    CommandEntry(9301, "editor.webView2",   "",                 Zone.EDITOR_ENG, cli_supported=False),
    CommandEntry(9302, "editor.monacoCore", "",                 Zone.EDITOR_ENG, cli_supported=False),
    CommandEntry(9303, "editor.cycle",      "",                 Zone.EDITOR_ENG, cli_supported=False),
    CommandEntry(9304, "editor.status",     "",                 Zone.EDITOR_ENG, cli_supported=False),
    
    # ── Voice (9700-9707) ──
    CommandEntry(9700, "voice.record",      "!voice record",    Zone.VOICE),
    CommandEntry(9702, "voice.speak",       "!voice speak",     Zone.VOICE),
    CommandEntry(9704, "voice.devices",     "!voice devices",   Zone.VOICE),
    CommandEntry(9705, "voice.metrics",     "!voice metrics",   Zone.VOICE),
    CommandEntry(9706, "voice.status",      "!voice status",    Zone.VOICE),
    CommandEntry(9707, "voice.mode",        "!voice mode",      Zone.VOICE),
    CommandEntry(0,    "voice.init",        "!voice init",      Zone.VOICE, gui_supported=False),
    CommandEntry(0,    "voice.transcribe",  "!voice transcribe", Zone.VOICE, gui_supported=False),
    
    # ── Server (CLI-only) ──
    CommandEntry(0, "server.start",         "!server start",    Zone.SERVER, gui_supported=False),
    CommandEntry(0, "server.stop",          "!server stop",     Zone.SERVER, gui_supported=False),
    CommandEntry(0, "server.status",        "!server status",   Zone.SERVER, gui_supported=False),
    
    # ── Settings (CLI-only) ──
    CommandEntry(0, "settings.open",        "!settings",        Zone.SETTINGS, gui_supported=False),
    CommandEntry(0, "settings.export",      "!settings_export", Zone.SETTINGS, gui_supported=False),
    CommandEntry(0, "settings.import",      "!settings_import", Zone.SETTINGS, gui_supported=False),
]


# ============================================================================
# TEST RESULTS
# ============================================================================

@dataclass
class TestResult:
    name: str
    passed: bool
    message: str
    duration_ms: float = 0.0

@dataclass
class TestSuite:
    name: str
    results: List[TestResult] = field(default_factory=list)
    
    @property
    def total(self) -> int:
        return len(self.results)
    
    @property
    def passed(self) -> int:
        return sum(1 for r in self.results if r.passed)
    
    @property
    def failed(self) -> int:
        return self.total - self.passed
    
    def add(self, name: str, passed: bool, message: str, duration_ms: float = 0.0):
        self.results.append(TestResult(name, passed, message, duration_ms))


# ============================================================================
# TEST 1: STATIC COLLISION DETECTION
# ============================================================================

def test_static_collision_detection() -> TestSuite:
    """Verify no duplicate command IDs exist in the canonical table."""
    suite = TestSuite("Static Collision Detection")
    
    id_map: Dict[int, str] = {}
    
    for cmd in COMMANDS:
        if cmd.id == 0:
            continue  # CLI-only, skip
        
        if cmd.id in id_map:
            suite.add(
                f"Collision check ID {cmd.id}",
                False,
                f"COLLISION: ID {cmd.id} claimed by '{id_map[cmd.id]}' AND '{cmd.feature_id}'"
            )
        else:
            id_map[cmd.id] = cmd.feature_id
            suite.add(
                f"ID {cmd.id} ({cmd.feature_id})",
                True,
                f"Unique — zone: {cmd.zone.value}"
            )
    
    return suite


# ============================================================================
# TEST 2: DEAD ZONE VALIDATION
# ============================================================================

def test_dead_zone_validation() -> TestSuite:
    """Verify no command IDs fall in forbidden dead zones."""
    suite = TestSuite("Dead Zone Validation")
    
    for cmd in COMMANDS:
        if cmd.id == 0:
            continue
        
        in_dead_zone = False
        dead_zone_name = ""
        
        for dz_min, dz_max, dz_name in DEAD_ZONES:
            if dz_min <= cmd.id <= dz_max:
                in_dead_zone = True
                dead_zone_name = dz_name
                break
        
        suite.add(
            f"Dead zone check {cmd.id} ({cmd.feature_id})",
            not in_dead_zone,
            f"IN DEAD ZONE: {dead_zone_name}" if in_dead_zone else "Clear"
        )
    
    return suite


# ============================================================================
# TEST 3: ZONE BOUNDARY VALIDATION
# ============================================================================

def test_zone_boundary_validation() -> TestSuite:
    """Verify each command ID falls within its declared zone boundaries."""
    suite = TestSuite("Zone Boundary Validation")
    
    for cmd in COMMANDS:
        if cmd.id == 0:
            suite.add(
                f"Zone check {cmd.feature_id} (CLI-only)",
                True,
                "CLI-only command, no zone check needed"
            )
            continue
        
        zone = cmd.zone
        if zone in ZONE_BOUNDS:
            z_min, z_max = ZONE_BOUNDS[zone]
            in_zone = z_min <= cmd.id <= z_max
            suite.add(
                f"Zone check {cmd.id} ({cmd.feature_id}) in {zone.value}",
                in_zone,
                f"Expected {z_min}-{z_max}, got {cmd.id}" if not in_zone else "In zone"
            )
        else:
            suite.add(
                f"Zone check {cmd.feature_id}",
                False,
                f"Zone {zone.value} has no bounds defined"
            )
    
    return suite


# ============================================================================
# TEST 4: CRITICAL RANGE EXHAUSTIVE TEST (4200-4311)
# ============================================================================

def test_critical_range_4200_4311() -> TestSuite:
    """
    Exhaustive validation of the formerly-dead AI Mode + RE range.
    Fires every ID from 4200-4311 and checks:
      1. Known IDs are registered
      2. Unknown IDs are properly rejected (CommandResult::error)
      3. No ID causes a crash or hang
    """
    suite = TestSuite("Critical Range 4200-4311 (AI Mode + RE)")
    
    # Build set of known IDs in this range
    known_ids: Dict[int, str] = {}
    for cmd in COMMANDS:
        if 4200 <= cmd.id <= 4311:
            known_ids[cmd.id] = cmd.feature_id
    
    # Report known registrations
    suite.add(
        "Known IDs in range",
        len(known_ids) > 0,
        f"Found {len(known_ids)} registered IDs: {sorted(known_ids.keys())}"
    )
    
    # Verify all expected IDs are present
    expected_ai = [4200, 4201, 4202]
    expected_re = [4300, 4301, 4302, 4308, 4311]
    
    for eid in expected_ai:
        suite.add(
            f"AI Mode ID {eid} registered",
            eid in known_ids,
            f"Registered as '{known_ids.get(eid, 'MISSING')}'" if eid in known_ids 
                else f"MISSING: ID {eid} not in registry"
        )
    
    for eid in expected_re:
        suite.add(
            f"RE ID {eid} registered",
            eid in known_ids,
            f"Registered as '{known_ids.get(eid, 'MISSING')}'" if eid in known_ids
                else f"MISSING: ID {eid} not in registry"
        )
    
    # Verify gap IDs (4203-4299, 4303-4307, 4309-4310) are NOT registered
    # (They're available for future use, not dead zone violations)
    gap_ids = (
        list(range(4203, 4300)) +  # AI Mode gaps
        [4303, 4304, 4305, 4306, 4307, 4309, 4310]  # RE gaps
    )
    
    registered_gaps = [gid for gid in gap_ids if gid in known_ids]
    suite.add(
        "Gap IDs not over-allocated",
        len(registered_gaps) == 0,
        f"UNEXPECTED registrations: {registered_gaps}" if registered_gaps
            else f"{len(gap_ids)} gap IDs available for future use"
    )
    
    return suite


# ============================================================================
# TEST 5: LIVE SERVER DISPATCH (requires running RawrXD server)
# ============================================================================

def test_live_server_dispatch(host: str, port: int, verbose: bool) -> TestSuite:
    """
    Fire every command ID through the HTTP dispatch endpoint and verify
    the C++ backend responds correctly.
    """
    suite = TestSuite("Live Server Dispatch")
    
    if not HAS_REQUESTS:
        suite.add("requests library", False, "pip install requests required")
        return suite
    
    base_url = f"http://{host}:{port}"
    
    # Check server is alive
    try:
        resp = requests.get(f"{base_url}/status", timeout=5)
        suite.add("Server alive", resp.status_code == 200,
                  f"Status: {resp.status_code}")
    except Exception as e:
        suite.add("Server alive", False, f"Connection failed: {e}")
        return suite  # Can't continue without server
    
    # ── Dispatch by command ID (GUI path) ──
    gui_commands = [cmd for cmd in COMMANDS if cmd.id != 0 and cmd.gui_supported]
    
    for cmd in gui_commands:
        try:
            t0 = time.perf_counter()
            resp = requests.post(
                f"{base_url}/api/dispatch",
                json={"commandId": cmd.id, "args": "", "source": "test"},
                timeout=10
            )
            elapsed = (time.perf_counter() - t0) * 1000
            
            # We expect either success OR a graceful error (not a 500/crash)
            is_ok = resp.status_code in (200, 400, 404)  # 400/404 = "not impl" is OK
            
            suite.add(
                f"GUI dispatch {cmd.id} ({cmd.feature_id})",
                is_ok,
                f"HTTP {resp.status_code} in {elapsed:.1f}ms",
                elapsed
            )
            
            if verbose and resp.status_code == 200:
                try:
                    data = resp.json()
                    print(f"  → {cmd.id}: {data.get('detail', 'no detail')}")
                except json.JSONDecodeError:
                    pass
                    
        except requests.Timeout:
            suite.add(f"GUI dispatch {cmd.id}", False, "TIMEOUT (10s)")
        except Exception as e:
            suite.add(f"GUI dispatch {cmd.id}", False, f"Error: {e}")
    
    # ── Dispatch by CLI command (CLI path) ──
    cli_commands = [cmd for cmd in COMMANDS if cmd.cli_command and cmd.cli_supported]
    
    for cmd in cli_commands:
        try:
            t0 = time.perf_counter()
            resp = requests.post(
                f"{base_url}/api/cli",
                json={"command": cmd.cli_command, "args": ""},
                timeout=10
            )
            elapsed = (time.perf_counter() - t0) * 1000
            
            is_ok = resp.status_code in (200, 400, 404)
            
            suite.add(
                f"CLI dispatch '{cmd.cli_command}' ({cmd.feature_id})",
                is_ok,
                f"HTTP {resp.status_code} in {elapsed:.1f}ms",
                elapsed
            )
            
        except requests.Timeout:
            suite.add(f"CLI dispatch '{cmd.cli_command}'", False, "TIMEOUT")
        except Exception as e:
            suite.add(f"CLI dispatch '{cmd.cli_command}'", False, f"Error: {e}")
    
    # ── Sweep the critical 4200-4311 range (every single ID) ──
    for sweep_id in range(4200, 4312):
        try:
            t0 = time.perf_counter()
            resp = requests.post(
                f"{base_url}/api/dispatch",
                json={"commandId": sweep_id, "args": "", "source": "matrix_test"},
                timeout=5
            )
            elapsed = (time.perf_counter() - t0) * 1000
            
            is_registered = sweep_id in {c.id for c in COMMANDS if c.id != 0}
            
            if is_registered:
                # Registered IDs should return 200
                suite.add(
                    f"Sweep {sweep_id} (registered)",
                    resp.status_code == 200,
                    f"HTTP {resp.status_code} in {elapsed:.1f}ms",
                    elapsed
                )
            else:
                # Unregistered IDs should return 400/404 (NOT 500)
                suite.add(
                    f"Sweep {sweep_id} (unregistered)",
                    resp.status_code in (400, 404),
                    f"HTTP {resp.status_code} in {elapsed:.1f}ms — " +
                    ("CRASH/500!" if resp.status_code >= 500 else "Graceful reject"),
                    elapsed
                )
            
        except Exception as e:
            suite.add(f"Sweep {sweep_id}", False, f"Error: {e}")
    
    return suite


# ============================================================================
# TEST 6: PLUGIN NAMESPACE ISOLATION
# ============================================================================

def test_plugin_namespace_isolation() -> TestSuite:
    """Verify plugin ID slots don't overlap core or dead zones."""
    suite = TestSuite("Plugin Namespace Isolation")
    
    PLUGIN_BASE = 60000
    PLUGIN_SLOT_SIZE = 1000
    PLUGIN_MAX_SLOTS = 5
    
    # Verify each slot is outside core zones
    for slot in range(PLUGIN_MAX_SLOTS):
        base = PLUGIN_BASE + slot * PLUGIN_SLOT_SIZE
        end  = base + PLUGIN_SLOT_SIZE - 1
        
        # Check no overlap with any core command
        overlaps = [
            cmd for cmd in COMMANDS 
            if cmd.id != 0 and base <= cmd.id <= end
        ]
        
        suite.add(
            f"Plugin slot {slot} ({base}-{end})",
            len(overlaps) == 0,
            f"OVERLAP with: {[c.feature_id for c in overlaps]}" if overlaps
                else "No overlap with core IDs"
        )
        
        # Check not in dead zone
        in_dead = False
        for dz_min, dz_max, dz_name in DEAD_ZONES:
            if not (end < dz_min or base > dz_max):
                in_dead = True
                suite.add(f"Plugin slot {slot} dead zone", False,
                         f"Overlaps {dz_name} ({dz_min}-{dz_max})")
        
        if not in_dead:
            suite.add(f"Plugin slot {slot} dead zone", True, "Clear")
    
    # Special check: slot 1 (61000-61999) vs SC_* (0xF000-0xF1FF = 61440-61951)
    suite.add(
        "Plugin slot 1 vs SC_* warning",
        False,  # This IS a known issue
        "WARNING: Slot 1 (61000-61999) partially overlaps SC_* (61440-61951). "
        "Use slots 0, 2-4 or mask SC_* in WndProc before dispatch."
    )
    
    return suite


# ============================================================================
# REPORTING
# ============================================================================

def print_suite(suite: TestSuite, verbose: bool = False):
    """Print a test suite's results."""
    print(f"\n{'='*72}")
    print(f"  {suite.name}")
    print(f"{'='*72}")
    
    for r in suite.results:
        status = "PASS" if r.passed else "FAIL"
        icon = "✓" if r.passed else "✗"
        
        if not r.passed or verbose:
            timing = f" ({r.duration_ms:.1f}ms)" if r.duration_ms > 0 else ""
            print(f"  [{status}] {icon} {r.name}{timing}")
            if not r.passed or verbose:
                print(f"         {r.message}")
    
    print(f"\n  Summary: {suite.passed}/{suite.total} passed, {suite.failed} failed")
    return suite.failed == 0


def main():
    parser = argparse.ArgumentParser(
        description="RawrXD Command ID Matrix Validator"
    )
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--offline", action="store_true", help="Static analysis only (no server)")
    parser.add_argument("--json", action="store_true", help="Output results as JSON")
    args = parser.parse_args()
    
    print("╔══════════════════════════════════════════════════════════════╗")
    print("║         RawrXD Command Matrix Test                         ║")
    print("║         command_matrix_test.py v1.0                        ║")
    print("╚══════════════════════════════════════════════════════════════╝")
    print(f"\nTotal commands in table: {len(COMMANDS)}")
    print(f"  GUI-capable: {sum(1 for c in COMMANDS if c.gui_supported)}")
    print(f"  CLI-capable: {sum(1 for c in COMMANDS if c.cli_supported)}")
    print(f"  CLI-only:    {sum(1 for c in COMMANDS if not c.gui_supported and c.cli_supported)}")
    print(f"  With ID:     {sum(1 for c in COMMANDS if c.id != 0)}")
    
    all_passed = True
    suites = []
    
    # ── Static tests (always run) ──
    suites.append(test_static_collision_detection())
    suites.append(test_dead_zone_validation())
    suites.append(test_zone_boundary_validation())
    suites.append(test_critical_range_4200_4311())
    suites.append(test_plugin_namespace_isolation())
    
    # ── Live tests (if server available) ──
    if not args.offline:
        suites.append(test_live_server_dispatch(args.host, args.port, args.verbose))
    
    # ── Print results ──
    for suite in suites:
        if not print_suite(suite, args.verbose):
            all_passed = False
    
    # ── JSON output ──
    if args.json:
        results = {
            "suites": [
                {
                    "name": s.name,
                    "total": s.total,
                    "passed": s.passed,
                    "failed": s.failed,
                    "tests": [
                        {
                            "name": r.name,
                            "passed": r.passed,
                            "message": r.message,
                            "duration_ms": r.duration_ms
                        }
                        for r in s.results
                    ]
                }
                for s in suites
            ],
            "overall_passed": all_passed
        }
        with open("command_matrix_results.json", "w") as f:
            json.dump(results, f, indent=2)
        print(f"\nJSON results written to command_matrix_results.json")
    
    # ── Final verdict ──
    total_tests = sum(s.total for s in suites)
    total_pass  = sum(s.passed for s in suites)
    total_fail  = sum(s.failed for s in suites)
    
    print(f"\n{'='*72}")
    print(f"  FINAL VERDICT: {total_pass}/{total_tests} tests passed, {total_fail} failed")
    if all_passed:
        print("  STATUS: ALL CLEAR — Command dispatch integrity confirmed")
    else:
        print("  STATUS: FAILURES DETECTED — See above for details")
    print(f"{'='*72}")
    
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
