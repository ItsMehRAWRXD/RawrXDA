#!/usr/bin/env python3
"""
Phase 2.5: Agentic Tool-Use Training Data Generator
====================================================
Generates synthetic multi-turn conversations where the AI uses IDE tools
autonomously — exactly like GitHub Copilot or Cursor agent does.

Each sample follows the format:
  system → user_request → assistant_think → tool_call → tool_result → assistant_response
  (potentially multiple tool-call/result rounds per conversation)

The model learns:
  1. WHEN to use which tool
  2. HOW to format tool calls (function-calling JSON)
  3. How to INTERPRET tool results
  4. How to CHAIN multiple tools for complex tasks
  5. How to RECOVER from failures
  6. How to PLAN multi-step operations

Output: ChatML-formatted JSONL compatible with the training pipeline.
"""

import json
import os
import sys
import random
import hashlib
import time
import re
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass, field, asdict
from concurrent.futures import ThreadPoolExecutor
import traceback

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEFAULT_CONFIG = {
    "corpus_dir": "F:\\RawrXD-AI-Training\\corpus",
    "raw_corpus": "F:\\RawrXD-AI-Training\\corpus\\raw_corpus.jsonl",
    "instructions_jsonl": "F:\\RawrXD-AI-Training\\corpus\\instructions.jsonl",
    "agentic_jsonl": "F:\\RawrXD-AI-Training\\corpus\\agentic_tool_use.jsonl",
    "tool_schema": "D:\\RawrXD-AI\\agentic_tool_schema.json",
    "ide_source_root": "D:\\rawrxd\\src",
    "max_samples": 50000,
    "max_turns_per_conversation": 12,
    "min_turns_per_conversation": 3,
    "seed": 42,
    "workers": 4,
}

SYSTEM_PROMPT = """You are RawrXD-AI, an expert autonomous coding agent integrated into the RawrXD IDE.
You have access to a comprehensive set of tools for reading, writing, searching, building, debugging, refactoring, and managing code.

Your capabilities include:
- Reading and editing any file in the workspace
- Searching code by text, regex, or semantic meaning
- Running terminal commands and build tasks
- Navigating code with LSP (goto definition, find references, symbols, call hierarchy)
- 20 built-in refactoring operations (extract method, rename, convert to modern C++, etc.)
- Autonomous debugging (crash analysis, watchpoints, git bisect, taint tracing)
- Three-layer hotpatching (memory, byte-level, server/inference)
- Knowledge graph queries (decisions, relationships, patterns, history)
- Autonomous workflows (compile-fix, lint-cleanup, security-audit, full-refactor)
- Failure detection and self-correction (8 failure types, 3 puppeteer strategies)
- Distributed swarm task execution
- Git operations (status, diff, commit)

RULES:
1. Always think before acting. Use the `think` tool to plan complex operations.
2. Read files before editing them to understand context.
3. After edits, verify correctness by checking errors or running builds.
4. Use the most specific tool available — don't shell out for things tools handle directly.
5. Chain tools intelligently: search → read → edit → verify → commit.
6. If a tool call fails, diagnose and retry with corrected parameters.
7. Record architectural decisions in the knowledge graph.
8. For multi-file changes, plan the full scope before starting edits.
9. Never guess file contents — always read first.
10. Return PatchResult-style responses: success status + detail message."""


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class ToolCall:
    name: str
    arguments: Dict[str, Any]

@dataclass
class ToolResult:
    name: str
    success: bool
    output: str

@dataclass
class ConversationTurn:
    role: str  # "system", "user", "assistant", "tool"
    content: str
    tool_calls: Optional[List[ToolCall]] = None
    tool_results: Optional[List[ToolResult]] = None

@dataclass
class AgenticConversation:
    turns: List[ConversationTurn] = field(default_factory=list)
    category: str = ""
    complexity: str = "simple"  # simple, medium, complex, expert
    tools_used: List[str] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Corpus cache — loads real code snippets for realistic examples
# ---------------------------------------------------------------------------

class CorpusCache:
    """Caches real code snippets from the raw corpus for use in synthetic examples."""

    def __init__(self, raw_corpus_path: str, max_entries: int = 5000):
        self.entries = []
        self.by_category = {}
        self.by_extension = {}
        self._load(raw_corpus_path, max_entries)

    def _load(self, path: str, max_entries: int):
        if not os.path.exists(path):
            print(f"  [WARN] Corpus not found at {path}, using synthetic examples only")
            return

        print(f"  Loading corpus cache from {path}...")
        count = 0
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                for line in f:
                    if count >= max_entries:
                        break
                    try:
                        entry = json.loads(line.strip())
                        self.entries.append(entry)
                        cat = entry.get("category", "unknown")
                        ext = entry.get("extension", "")
                        self.by_category.setdefault(cat, []).append(entry)
                        self.by_extension.setdefault(ext, []).append(entry)
                        count += 1
                    except json.JSONDecodeError:
                        continue
        except Exception as e:
            print(f"  [WARN] Error loading corpus: {e}")

        print(f"  Loaded {count} corpus entries ({len(self.by_category)} categories)")

    def random_entry(self, category: str = None, extension: str = None):
        pool = self.entries
        if category and category in self.by_category:
            pool = self.by_category[category]
        elif extension and extension in self.by_extension:
            pool = self.by_extension[extension]
        if not pool:
            return self._synthetic_entry()
        return random.choice(pool)

    def random_code(self, category: str = None, max_lines: int = 50):
        entry = self.random_entry(category=category)
        content = entry.get("content", "// empty")
        lines = content.split("\n")
        if len(lines) > max_lines:
            start = random.randint(0, len(lines) - max_lines)
            lines = lines[start:start + max_lines]
        return "\n".join(lines), entry.get("path", "src/unknown.cpp")

    def random_file_path(self, category: str = None):
        entry = self.random_entry(category=category)
        return entry.get("path", "src/main.cpp")

    def _synthetic_entry(self):
        return {
            "path": "src/core/example.cpp",
            "content": '#include "example.hpp"\n\nvoid Example::run() {\n    // TODO: implement\n}\n',
            "category": "code",
            "extension": ".cpp"
        }


# ---------------------------------------------------------------------------
# Scenario Generators — each produces a full agentic conversation
# ---------------------------------------------------------------------------

class ScenarioGenerator:
    """Base class for scenario generators."""

    def __init__(self, corpus: CorpusCache, rng: random.Random):
        self.corpus = corpus
        self.rng = rng

    def _think(self, reasoning: str) -> ConversationTurn:
        tc = ToolCall(name="think", arguments={"reasoning": reasoning})
        return ConversationTurn(
            role="assistant",
            content="",
            tool_calls=[tc]
        )

    def _tool_call(self, name: str, args: Dict) -> ToolCall:
        return ToolCall(name=name, arguments=args)

    def _tool_result(self, name: str, output: str, success: bool = True) -> ConversationTurn:
        tr = ToolResult(name=name, success=success, output=output)
        return ConversationTurn(role="tool", content="", tool_results=[tr])

    def _assistant(self, content: str, tool_calls: List[ToolCall] = None) -> ConversationTurn:
        return ConversationTurn(role="assistant", content=content, tool_calls=tool_calls)

    def _user(self, content: str) -> ConversationTurn:
        return ConversationTurn(role="user", content=content)


class ReadEditVerifyScenario(ScenarioGenerator):
    """Read a file → understand it → make a targeted edit → verify the change."""

    EDIT_TASKS = [
        ("Add error handling to {func}", "add null checks and return PatchResult::error on failure"),
        ("Add logging to {func}", "add structured logging at entry/exit points"),
        ("Fix the {issue} in {func}", "correct the off-by-one / null deref / missing return"),
        ("Optimize {func} for performance", "replace linear scan with binary search / SIMD"),
        ("Add const correctness to {func}", "mark immutable params as const, use const&"),
        ("Convert {func} to use RAII", "wrap raw pointers in unique_ptr/lock_guard"),
        ("Add documentation to {func}", "add doxygen comments with @param/@return/@brief"),
        ("Thread-safe {func}", "add mutex locking with std::lock_guard"),
        ("Add assertion checks to {func}", "add precondition/postcondition asserts"),
        ("Modernize {func} to C++20", "use ranges, concepts, structured bindings"),
    ]

    def generate(self) -> AgenticConversation:
        code, filepath = self.corpus.random_code(category="code", max_lines=40)
        funcs = re.findall(r'(\w+)\s*\(', code)
        func = self.rng.choice(funcs) if funcs else "processData"
        task_template, detail = self.rng.choice(self.EDIT_TASKS)
        issue = self.rng.choice(["bug", "memory leak", "race condition", "undefined behavior"])
        task = task_template.format(func=func, issue=issue)

        conv = AgenticConversation(category="read_edit_verify", complexity="medium")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(task + f" in `{filepath}`"))

        # Think step
        conv.turns.append(self._think(
            f"The user wants me to {task.lower()} in {filepath}. "
            f"I need to: 1) Read the file to understand the current implementation, "
            f"2) Make the targeted edit, 3) Verify no errors were introduced."
        ))

        # Read file
        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("read_file", {"path": filepath})
        ]))
        conv.turns.append(self._tool_result("read_file", code))

        # Edit
        lines = code.split("\n")
        edit_line = max(1, len(lines) // 2)
        old_snippet = "\n".join(lines[edit_line-1:edit_line+2]) if len(lines) > edit_line+2 else lines[-1]
        new_snippet = f"    // {detail}\n{old_snippet}"

        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("edit_file", {
                "path": filepath,
                "old_text": old_snippet,
                "new_text": new_snippet
            })
        ]))
        conv.turns.append(self._tool_result("edit_file", f"Successfully edited {filepath}"))

        # Verify
        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("get_errors", {"file_paths": [filepath]})
        ]))
        conv.turns.append(self._tool_result("get_errors", "No errors found."))

        conv.turns.append(self._assistant(
            f"Done. I've {task.lower()} in `{filepath}`. "
            f"The change {detail}. No compile errors detected."
        ))

        conv.tools_used = ["think", "read_file", "edit_file", "get_errors"]
        return conv


class SearchAndRefactorScenario(ScenarioGenerator):
    """Search for a pattern → find files → refactor across them."""

    def generate(self) -> AgenticConversation:
        refactorings = [
            ("rename", "rename_symbol", "Rename `{old}` to `{new}` across the codebase"),
            ("convert_for", "convert_for_to_range_for", "Convert all raw for loops to range-based for"),
            ("smart_ptr", "convert_raw_to_smart_ptr", "Convert raw pointers to smart pointers"),
            ("includes", "organize_includes", "Organize and sort all #include directives"),
            ("auto", "convert_to_auto", "Convert obvious type declarations to auto"),
        ]

        refactor_type, refactor_op, desc_template = self.rng.choice(refactorings)
        old_name = self.rng.choice(["getData", "processItem", "handleEvent", "createWidget", "parseInput"])
        new_name = self.rng.choice(["fetchData", "processEntry", "onEvent", "buildWidget", "deserializeInput"])
        desc = desc_template.format(old=old_name, new=new_name)

        conv = AgenticConversation(category="search_refactor", complexity="complex")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(desc))

        # Think
        conv.turns.append(self._think(
            f"This is a codebase-wide refactoring task. I need to: "
            f"1) Search for all occurrences of the target pattern, "
            f"2) Understand the scope of changes needed, "
            f"3) Apply the refactoring, "
            f"4) Verify no breakage."
        ))

        # Search
        conv.turns.append(self._assistant("Let me first search for all relevant occurrences.", tool_calls=[
            self._tool_call("search_files", {"query": old_name, "include_pattern": "**/*.{cpp,hpp,h}"})
        ]))

        file_paths = [self.corpus.random_file_path(category="code") for _ in range(self.rng.randint(2, 5))]
        search_results = "\n".join([f"  {fp}:L{self.rng.randint(10,200)}: ...{old_name}..." for fp in file_paths])
        conv.turns.append(self._tool_result("search_files", f"Found {len(file_paths)} matches:\n{search_results}"))

        # Find references via LSP
        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("find_references", {"symbol": old_name, "file": file_paths[0]})
        ]))
        ref_results = "\n".join([f"  {fp}:L{self.rng.randint(10,200)}" for fp in file_paths])
        conv.turns.append(self._tool_result("find_references", f"{len(file_paths)} references found:\n{ref_results}"))

        # Apply refactoring
        if refactor_type == "rename":
            conv.turns.append(self._assistant(
                f"Found {len(file_paths)} files with references. Applying rename refactoring.",
                tool_calls=[self._tool_call("refactor", {
                    "operation": refactor_op,
                    "file": file_paths[0],
                    "new_name": new_name
                })]
            ))
        else:
            conv.turns.append(self._assistant(
                f"Found {len(file_paths)} files. Applying {refactor_op} refactoring.",
                tool_calls=[self._tool_call("bulk_refactor", {
                    "strategy": "refactor_rename" if refactor_type == "rename" else "lint_fix",
                    "file_paths": file_paths
                })]
            ))

        conv.turns.append(self._tool_result("refactor",
            f"Refactoring applied successfully to {len(file_paths)} files. "
            f"{len(file_paths) * self.rng.randint(2,8)} changes made."
        ))

        # Build to verify
        conv.turns.append(self._assistant("Let me build to verify everything compiles.", tool_calls=[
            self._tool_call("run_build", {"target": "RawrXD-Shell", "config": "Release"})
        ]))
        conv.turns.append(self._tool_result("run_build", "Build succeeded. 0 errors, 0 warnings."))

        # Commit
        conv.turns.append(self._assistant("Build passes. Committing the changes.", tool_calls=[
            self._tool_call("git_commit", {
                "message": f"refactor: {desc}",
                "files": file_paths
            })
        ]))
        conv.turns.append(self._tool_result("git_commit", f"[main abc1234] refactor: {desc}\n {len(file_paths)} files changed"))

        conv.turns.append(self._assistant(
            f"Refactoring complete. {desc}. "
            f"Changed {len(file_paths)} files, build passes, committed as abc1234."
        ))

        conv.tools_used = ["think", "search_files", "find_references", "refactor", "run_build", "git_commit"]
        return conv


class DebugCrashScenario(ScenarioGenerator):
    """Debug a crash: analyze → set watchpoints → bisect → fix → verify."""

    def generate(self) -> AgenticConversation:
        crash_types = [
            ("access violation", "nullptr dereference in renderWidget", "EXCEPTION_ACCESS_VIOLATION"),
            ("stack overflow", "infinite recursion in parseExpression", "EXCEPTION_STACK_OVERFLOW"),
            ("heap corruption", "double-free in CommandRegistry::unregister", "heap-buffer-overflow"),
            ("assertion failure", "size != 0 failed in ByteLevelHotpatcher::apply", "assert_failure"),
            ("segfault", "out-of-bounds tensor access in memory_hotpatch", "SIGSEGV"),
        ]

        crash_name, crash_desc, crash_code = self.rng.choice(crash_types)
        func_name = crash_desc.split(" in ")[-1] if " in " in crash_desc else "main"
        filepath = self.corpus.random_file_path(category="code")

        conv = AgenticConversation(category="debug_crash", complexity="expert")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(
            f"The IDE crashed with '{crash_code}' — something about {crash_desc}. Can you investigate and fix it?"
        ))

        # Think
        conv.turns.append(self._think(
            f"Crash analysis needed. The {crash_code} suggests {crash_name}. "
            f"I'll: 1) Analyze the crash dump, 2) Read the offending code, "
            f"3) Set watchpoints if needed, 4) Trace backwards to root cause, "
            f"5) Apply fix, 6) Verify."
        ))

        # Analyze crash
        conv.turns.append(self._assistant("Let me analyze the crash report.", tool_calls=[
            self._tool_call("debug_analyze_crash", {
                "stack_trace": f"#0 {func_name}() at {filepath}:42\n#1 main() at src/main.cpp:100"
            })
        ]))
        conv.turns.append(self._tool_result("debug_analyze_crash",
            f"Root Cause Analysis:\n"
            f"  Classification: {crash_name}\n"
            f"  Confidence: 0.92\n"
            f"  Location: {filepath}:42 in {func_name}()\n"
            f"  Likely cause: Unvalidated pointer before dereference\n"
            f"  Suggested fix: Add null check before access"
        ))

        # Read the offending code
        code, _ = self.corpus.random_code(category="code", max_lines=30)
        conv.turns.append(self._assistant("Let me read the code around the crash site.", tool_calls=[
            self._tool_call("read_file", {"path": filepath, "start_line": 30, "end_line": 60})
        ]))
        conv.turns.append(self._tool_result("read_file", code))

        # Trace backwards
        conv.turns.append(self._assistant("Tracing the variable backwards to find the source.", tool_calls=[
            self._tool_call("debug_trace_backwards", {
                "variable": "ptr",
                "file": filepath,
                "line": 42
            })
        ]))
        conv.turns.append(self._tool_result("debug_trace_backwards",
            f"Taint chain for 'ptr':\n"
            f"  {filepath}:42 — dereferenced (CRASH)\n"
            f"  {filepath}:38 — assigned from getBuffer() return value\n"
            f"  {filepath}:25 — getBuffer() can return nullptr when cache is empty\n"
            f"  Root: Missing null check after getBuffer() call"
        ))

        # Fix
        conv.turns.append(self._assistant(
            "Found the root cause: `getBuffer()` can return nullptr when cache is empty. Adding null check.",
            tool_calls=[self._tool_call("edit_file", {
                "path": filepath,
                "old_text": "    auto* ptr = getBuffer();\n    ptr->process();",
                "new_text": "    auto* ptr = getBuffer();\n    if (!ptr) {\n        return PatchResult::error(\"Buffer is null — cache empty\");\n    }\n    ptr->process();"
            })]
        ))
        conv.turns.append(self._tool_result("edit_file", f"Successfully edited {filepath}"))

        # Build & verify
        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("run_build", {"target": "RawrXD-Shell", "config": "Debug"})
        ]))
        conv.turns.append(self._tool_result("run_build", "Build succeeded. 0 errors, 2 warnings."))

        # Knowledge graph
        conv.turns.append(self._assistant("Recording this fix in the knowledge graph.", tool_calls=[
            self._tool_call("knowledge_record", {
                "type": "BugFix",
                "description": f"Fixed {crash_name} in {func_name}: added null check after getBuffer()",
                "file": filepath,
                "rationale": "getBuffer() can return nullptr when the cache is empty, causing a crash on dereference"
            })
        ]))
        conv.turns.append(self._tool_result("knowledge_record", "Decision recorded in knowledge graph."))

        conv.turns.append(self._assistant(
            f"Fixed the {crash_name} in `{func_name}`. Root cause: `getBuffer()` returns nullptr "
            f"when cache is empty, and the result wasn't checked before dereference. "
            f"Added a null check that returns `PatchResult::error()` with a descriptive message. "
            f"Build passes. Recorded in knowledge graph for future reference."
        ))

        conv.tools_used = ["think", "debug_analyze_crash", "read_file", "debug_trace_backwards",
                           "edit_file", "run_build", "knowledge_record"]
        return conv


class BuildFixLoopScenario(ScenarioGenerator):
    """Build fails → read errors → fix → rebuild → repeat until success."""

    def generate(self) -> AgenticConversation:
        errors = [
            ("error C2664", "cannot convert 'const char*' to 'char*'", "add const_cast or fix signature"),
            ("error C2039", "'value_type' is not a member of 'std::vector'", "use typename or value_type alias"),
            ("error LNK2001", "unresolved external symbol", "implement missing function or add .asm to target"),
            ("error C2440", "cannot convert from 'initializer list' to 'PatchResult'", "use factory method PatchResult::ok()"),
            ("error C3861", "'apply_hotpatch' identifier not found", "add #include or forward declaration"),
        ]

        err_code, err_msg, fix_desc = self.rng.choice(errors)
        filepath = self.corpus.random_file_path(category="code")

        conv = AgenticConversation(category="build_fix", complexity="medium")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user("The build is failing. Fix it."))

        # Think
        conv.turns.append(self._think(
            "Build failure reported. I'll run a build to get exact errors, "
            "then fix them iteratively until the build succeeds."
        ))

        # Build (fails)
        conv.turns.append(self._assistant("Running a build to see the errors.", tool_calls=[
            self._tool_call("run_build", {"target": "RawrXD-Shell", "config": "Release"})
        ]))
        conv.turns.append(self._tool_result("run_build",
            f"Build FAILED. 1 error, 0 warnings.\n"
            f"{filepath}(42): {err_code}: {err_msg}",
            success=False
        ))

        # Read the erroneous code
        code, _ = self.corpus.random_code(category="code", max_lines=20)
        conv.turns.append(self._assistant(f"Build error `{err_code}` in {filepath}. Let me read the code.", tool_calls=[
            self._tool_call("read_file", {"path": filepath, "start_line": 35, "end_line": 55})
        ]))
        conv.turns.append(self._tool_result("read_file", code))

        # Fix
        conv.turns.append(self._assistant(
            f"I see the issue: {err_msg}. I need to {fix_desc}.",
            tool_calls=[self._tool_call("edit_file", {
                "path": filepath,
                "old_text": code.split("\n")[3] if len(code.split("\n")) > 3 else code,
                "new_text": f"    // Fixed: {fix_desc}\n" + (code.split("\n")[3] if len(code.split("\n")) > 3 else code)
            })]
        ))
        conv.turns.append(self._tool_result("edit_file", f"Successfully edited {filepath}"))

        # Rebuild (succeeds)
        conv.turns.append(self._assistant("Rebuilding to verify the fix.", tool_calls=[
            self._tool_call("run_build", {"target": "RawrXD-Shell", "config": "Release"})
        ]))
        conv.turns.append(self._tool_result("run_build", "Build succeeded. 0 errors, 0 warnings."))

        conv.turns.append(self._assistant(
            f"Build fixed. The error `{err_code}: {err_msg}` was caused by a type mismatch. "
            f"I {fix_desc}. Build now passes cleanly."
        ))

        conv.tools_used = ["think", "run_build", "read_file", "edit_file"]
        return conv


class HotpatchScenario(ScenarioGenerator):
    """Apply hotpatches at memory/byte/server layers."""

    def generate(self) -> AgenticConversation:
        layers = [
            ("memory", "Patch the temperature parameter in the loaded model from 0.7 to 0.3"),
            ("byte", "Patch the GGUF metadata to update the model name field"),
            ("server", "Add a pre-request transform that injects a system prompt"),
        ]

        layer, task = self.rng.choice(layers)

        conv = AgenticConversation(category="hotpatch", complexity="expert")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(task))

        conv.turns.append(self._think(
            f"This is a {layer}-layer hotpatch operation. "
            f"I need to use the hotpatch_{layer} tool to apply this change at runtime "
            f"without restarting the server or reloading the model."
        ))

        if layer == "memory":
            conv.turns.append(self._assistant("Applying memory-level hotpatch to the loaded model.", tool_calls=[
                self._tool_call("hotpatch_memory", {
                    "address": "0x7FF6A0001000",
                    "size": 4,
                    "data": "9A99993E",  # 0.3f in little-endian
                    "layer": "temperature_param"
                })
            ]))
            conv.turns.append(self._tool_result("hotpatch_memory",
                "PatchResult::ok — Memory patch applied at 0x7FF6A0001000 (4 bytes). "
                "VirtualProtect: PAGE_READWRITE → patched → PAGE_READONLY restored."
            ))
        elif layer == "byte":
            conv.turns.append(self._assistant("Applying byte-level patch to the GGUF file.", tool_calls=[
                self._tool_call("hotpatch_bytes", {
                    "filename": "D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-33B-Q2_K.gguf",
                    "pattern": "general.name",
                    "data": "52617772584420414900",  # "RawrXD AI\0"
                    "mutation": "replace"
                })
            ]))
            conv.turns.append(self._tool_result("hotpatch_bytes",
                "PatchResult::ok — Byte patch applied. Pattern 'general.name' found at offset 0x120. "
                "10 bytes written via mmap. File integrity verified."
            ))
        else:
            conv.turns.append(self._assistant("Adding server-layer request transform.", tool_calls=[
                self._tool_call("hotpatch_server", {
                    "name": "system_prompt_injector",
                    "injection_point": "PreRequest",
                    "transform_code": 'req->prompt = "You are RawrXD-AI.\\n" + req->prompt; return true;'
                })
            ]))
            conv.turns.append(self._tool_result("hotpatch_server",
                "PatchResult::ok — Server hotpatch 'system_prompt_injector' registered at PreRequest. "
                "Hit count: 0 (will apply to next request)."
            ))

        conv.turns.append(self._assistant(
            f"Hotpatch applied successfully at the {layer} layer. {task}. "
            f"No restart required — the change is active immediately."
        ))

        conv.tools_used = ["think", f"hotpatch_{layer}"]
        return conv


class AutonomousWorkflowScenario(ScenarioGenerator):
    """Execute a full autonomous workflow with planning, execution, and verification."""

    def generate(self) -> AgenticConversation:
        workflows = [
            ("compile_fix", "Fix all compile errors in the project",
             "Found 3 errors across 2 files. Applied fixes: const correctness, missing include, factory method."),
            ("lint_cleanup", "Clean up all lint warnings",
             "Fixed 12 warnings across 8 files: unused variables, missing braces, implicit conversions."),
            ("security_audit", "Run a security audit on the codebase",
             "Found 2 issues: unvalidated input in parseCommand, missing bounds check in bufferCopy."),
            ("full_refactor", "Modernize the hotpatch subsystem to C++20",
             "Converted 5 files: concepts for template constraints, ranges for iteration, designated initializers."),
        ]

        wf_type, task, result_desc = self.rng.choice(workflows)

        conv = AgenticConversation(category="autonomous_workflow", complexity="expert")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(task))

        # Plan
        conv.turns.append(self._think(
            f"This requires a full autonomous workflow. I'll use the '{wf_type}' workflow "
            f"which handles: snapshot → analyze → fix → verify → commit. "
            f"The workflow has auto-rollback on failure."
        ))

        conv.turns.append(self._assistant(f"Launching the `{wf_type}` autonomous workflow.", tool_calls=[
            self._tool_call("run_workflow", {
                "workflow": wf_type,
                "policy": {"max_retries": 3, "timeout_ms": 300000, "auto_rollback": True}
            })
        ]))
        conv.turns.append(self._tool_result("run_workflow",
            f"Workflow '{wf_type}' completed successfully.\n"
            f"Stage progression: Analyze → Fix → Verify → Complete\n"
            f"Result: {result_desc}\n"
            f"Snapshot taken — can rollback if needed."
        ))

        # Verify with build
        conv.turns.append(self._assistant("Verifying with a full build.", tool_calls=[
            self._tool_call("run_build", {"target": "RawrXD-Shell", "config": "Release"})
        ]))
        conv.turns.append(self._tool_result("run_build", "Build succeeded. 0 errors, 0 warnings."))

        conv.turns.append(self._assistant(
            f"Workflow complete. {result_desc} Build passes. "
            f"A snapshot was taken before changes — rollback is available if needed."
        ))

        conv.tools_used = ["think", "run_workflow", "run_build"]
        return conv


class FailureRecoveryScenario(ScenarioGenerator):
    """Detect a failure in model output → correct it → retry."""

    def generate(self) -> AgenticConversation:
        failures = [
            ("Refusal", "I cannot help with that request.", "The model refused a legitimate code request"),
            ("Hallucination", "Use QWidget::setLayout() to configure the hotpatch.",
             "The model hallucinated a Qt API — we don't use Qt"),
            ("FormatViolation", "Here's the code: function main() { ... }",
             "The model generated JavaScript instead of C++"),
            ("Timeout", "[no response after 30s]", "The model timed out during generation"),
        ]

        failure_type, bad_output, reason = self.rng.choice(failures)

        conv = AgenticConversation(category="failure_recovery", complexity="complex")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user("Generate a function to patch tensor weights in memory"))

        # First attempt (fails)
        conv.turns.append(self._assistant(bad_output))

        # Detect failure
        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("detect_failure", {
                "output": bad_output,
                "expected_format": "C++ function with PatchResult return type"
            })
        ]))
        conv.turns.append(self._tool_result("detect_failure",
            f"Failure detected:\n"
            f"  Type: {failure_type}\n"
            f"  Confidence: 0.95\n"
            f"  Reason: {reason}"
        ))

        # Correct
        conv.turns.append(self._assistant("Failure detected. Applying correction.", tool_calls=[
            self._tool_call("correct_response", {
                "failed_output": bad_output,
                "failure_type": failure_type,
                "correction_prompt": "Generate a C++ function using VirtualProtect to patch tensor weights. Return PatchResult."
            })
        ]))
        conv.turns.append(self._tool_result("correct_response",
            "Correction applied. Regenerated response:\n\n"
            "```cpp\n"
            "PatchResult patchTensorWeights(void* addr, size_t size, const void* newWeights) {\n"
            "    DWORD oldProtect;\n"
            "    if (!VirtualProtect(addr, size, PAGE_READWRITE, &oldProtect)) {\n"
            "        return PatchResult::error(\"VirtualProtect failed\", GetLastError());\n"
            "    }\n"
            "    memcpy(addr, newWeights, size);\n"
            "    VirtualProtect(addr, size, oldProtect, &oldProtect);\n"
            "    return PatchResult::ok(\"Tensor weights patched\");\n"
            "}\n"
            "```"
        ))

        conv.turns.append(self._assistant(
            "Here's the corrected function:\n\n```cpp\n"
            "PatchResult patchTensorWeights(void* addr, size_t size, const void* newWeights) {\n"
            "    DWORD oldProtect;\n"
            "    if (!VirtualProtect(addr, size, PAGE_READWRITE, &oldProtect)) {\n"
            "        return PatchResult::error(\"VirtualProtect failed\", GetLastError());\n"
            "    }\n"
            "    memcpy(addr, newWeights, size);\n"
            "    VirtualProtect(addr, size, oldProtect, &oldProtect);\n"
            "    return PatchResult::ok(\"Tensor weights patched\");\n"
            "}\n```\n\n"
            "This uses the IDE's three-layer hotpatch pattern: VirtualProtect → memcpy → restore. "
            "Returns `PatchResult` as per project conventions."
        ))

        conv.tools_used = ["detect_failure", "correct_response"]
        return conv


class MultiFileImplementScenario(ScenarioGenerator):
    """Implement a new feature spanning header + source + test files."""

    def generate(self) -> AgenticConversation:
        features = [
            ("TokenBiasInjector", "Inject token probability biases during inference",
             "core", ["token_bias.hpp", "token_bias.cpp"]),
            ("ModelVersionManager", "Track and switch between model versions",
             "core", ["model_version_mgr.hpp", "model_version_mgr.cpp"]),
            ("InferenceCacheLayer", "Cache repeated inference results for identical prompts",
             "server", ["inference_cache.hpp", "inference_cache.cpp"]),
            ("AgentMemoryStore", "Persistent memory across agent sessions",
             "agent", ["agent_memory.hpp", "agent_memory.cpp"]),
        ]

        class_name, desc, subdir, files = self.rng.choice(features)

        conv = AgenticConversation(category="multi_file_implement", complexity="expert")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(f"Implement a new `{class_name}` class: {desc}"))

        # Plan
        conv.turns.append(self._think(
            f"Implementing {class_name} requires:\n"
            f"1. Create header in src/{subdir}/{files[0]} with class declaration\n"
            f"2. Create source in src/{subdir}/{files[1]} with implementation\n"
            f"3. Follow project patterns: singleton, PatchResult returns, no exceptions\n"
            f"4. Add to CMakeLists.txt\n"
            f"5. Build and verify"
        ))

        conv.turns.append(self._assistant(
            "", tool_calls=[self._tool_call("manage_plan", {"tasks": [
                {"id": 1, "title": f"Create {files[0]} header", "status": "not-started"},
                {"id": 2, "title": f"Create {files[1]} implementation", "status": "not-started"},
                {"id": 3, "title": "Update CMakeLists.txt", "status": "not-started"},
                {"id": 4, "title": "Build and verify", "status": "not-started"},
            ]})]
        ))
        conv.turns.append(self._tool_result("manage_plan", "Plan created with 4 tasks."))

        # Check existing patterns
        conv.turns.append(self._assistant("Let me check existing patterns in the codebase.", tool_calls=[
            self._tool_call("semantic_search", {"query": f"singleton class pattern with PatchResult in {subdir}"})
        ]))
        conv.turns.append(self._tool_result("semantic_search",
            f"Found pattern in src/{subdir}/model_memory_hotpatch.hpp:\n"
            f"  class ModelMemoryHotpatch {{\n"
            f"    static ModelMemoryHotpatch& instance();\n"
            f"    PatchResult apply(...);\n"
            f"  }};\n"
        ))

        # Create header
        header_content = (
            f'#pragma once\n'
            f'#include "patch_result.h"\n'
            f'#include <string>\n'
            f'#include <mutex>\n\n'
            f'namespace RawrXD {{\n\n'
            f'class {class_name} {{\n'
            f'public:\n'
            f'    static {class_name}& instance();\n'
            f'    PatchResult initialize();\n'
            f'    PatchResult shutdown();\n'
            f'    // Core functionality\n'
            f'    PatchResult process(const std::string& input);\n'
            f'private:\n'
            f'    {class_name}() = default;\n'
            f'    std::mutex m_mutex;\n'
            f'    bool m_initialized = false;\n'
            f'}};\n\n'
            f'}} // namespace RawrXD\n'
        )

        conv.turns.append(self._assistant(f"Creating the header file.", tool_calls=[
            self._tool_call("write_file", {
                "path": f"src/{subdir}/{files[0]}",
                "content": header_content
            })
        ]))
        conv.turns.append(self._tool_result("write_file", f"Created src/{subdir}/{files[0]}"))

        # Create source
        source_content = (
            f'#include "{files[0]}"\n'
            f'#include <lock_guard>\n\n'
            f'namespace RawrXD {{\n\n'
            f'{class_name}& {class_name}::instance() {{\n'
            f'    static {class_name} s;\n'
            f'    return s;\n'
            f'}}\n\n'
            f'PatchResult {class_name}::initialize() {{\n'
            f'    std::lock_guard<std::mutex> lock(m_mutex);\n'
            f'    if (m_initialized) return PatchResult::ok("Already initialized");\n'
            f'    m_initialized = true;\n'
            f'    return PatchResult::ok("{class_name} ready");\n'
            f'}}\n\n'
            f'PatchResult {class_name}::process(const std::string& input) {{\n'
            f'    std::lock_guard<std::mutex> lock(m_mutex);\n'
            f'    if (!m_initialized) return PatchResult::error("Not initialized");\n'
            f'    // TODO: implement core logic\n'
            f'    return PatchResult::ok("Processed");\n'
            f'}}\n\n'
            f'}} // namespace RawrXD\n'
        )

        conv.turns.append(self._assistant(f"Creating the implementation file.", tool_calls=[
            self._tool_call("write_file", {
                "path": f"src/{subdir}/{files[1]}",
                "content": source_content
            })
        ]))
        conv.turns.append(self._tool_result("write_file", f"Created src/{subdir}/{files[1]}"))

        # Update CMakeLists
        conv.turns.append(self._assistant("Adding to CMakeLists.txt.", tool_calls=[
            self._tool_call("edit_file", {
                "path": "CMakeLists.txt",
                "old_text": "# Source files\nset(SOURCES",
                "new_text": f"# Source files\nset(SOURCES\n    src/{subdir}/{files[1]}"
            })
        ]))
        conv.turns.append(self._tool_result("edit_file", "Successfully edited CMakeLists.txt"))

        # Build
        conv.turns.append(self._assistant("Building to verify.", tool_calls=[
            self._tool_call("run_build", {"target": "RawrXD-Shell", "config": "Release"})
        ]))
        conv.turns.append(self._tool_result("run_build", "Build succeeded. 0 errors, 0 warnings."))

        conv.turns.append(self._assistant(
            f"`{class_name}` implemented. Created:\n"
            f"- `src/{subdir}/{files[0]}` — Header with singleton pattern, PatchResult API\n"
            f"- `src/{subdir}/{files[1]}` — Implementation with mutex locking\n"
            f"Build passes. The class follows project conventions: no exceptions, "
            f"PatchResult returns, singleton via static instance()."
        ))

        conv.tools_used = ["think", "manage_plan", "semantic_search", "write_file", "edit_file", "run_build"]
        return conv


class TerminalAndBuildScenario(ScenarioGenerator):
    """Execute terminal commands: cmake configure, build, run tests, manage processes."""

    def generate(self) -> AgenticConversation:
        tasks = [
            ("Configure and build the project from scratch",
             [("run_terminal", {"command": "cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release"}, "CMake configured successfully"),
              ("run_terminal", {"command": "cmake --build build --config Release -j16"}, "Build succeeded: 248 targets built"),
              ("run_terminal", {"command": ".\\build\\RawrXD-Shell.exe --self-test"}, "All 42 tests passed")]),
            ("Run the model server with custom parameters",
             [("run_terminal", {"command": "D:\\OllamaModels\\llama.cpp\\llama-server.exe -m D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-33B-Q2_K.gguf -ngl 99 -c 8192 --port 8080", "background": True},
               "Server started on http://127.0.0.1:8080"),
              ("run_terminal", {"command": "curl -s http://127.0.0.1:8080/health"}, '{"status":"ok"}')]),
            ("Profile memory usage during inference",
             [("run_terminal", {"command": "Get-Process llama-server | Select-Object WorkingSet64,VirtualMemorySize64"}, "WorkingSet64: 14.2GB"),
              ("run_terminal", {"command": "D:\\OllamaModels\\llama.cpp\\llama-cli.exe -m D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-33B-Q2_K.gguf --log-disable -p 'Hello' -n 1 2>&1 | Select-String 'llama_print_timings'"}, "eval time = 85.42ms per token")]),
        ]

        task_desc, steps = self.rng.choice(tasks)

        conv = AgenticConversation(category="terminal_build", complexity="medium")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(task_desc))

        conv.turns.append(self._think(
            f"User wants to: {task_desc}. "
            f"I'll execute the necessary terminal commands in sequence."
        ))

        for tool_name, args, result in steps:
            conv.turns.append(self._assistant("", tool_calls=[self._tool_call(tool_name, args)]))
            conv.turns.append(self._tool_result(tool_name, result))

        conv.turns.append(self._assistant(f"Done. {task_desc} completed successfully."))
        conv.tools_used = ["think", "run_terminal"]
        return conv


class LSPNavigationScenario(ScenarioGenerator):
    """Use LSP tools to navigate and understand code: goto def, references, call hierarchy."""

    def generate(self) -> AgenticConversation:
        symbols = [
            ("apply_memory_patch", "model_memory_hotpatch.cpp", "core"),
            ("CommandRegistry::execute", "command_registry.cpp", "ide"),
            ("ToolRegistry::executeTool", "tool_registry.hpp", "agent"),
            ("CLIAutonomyLoop::tick", "cli_autonomy_loop.hpp", "cli"),
            ("KnowledgeGraphCore::semanticSearch", "knowledge_graph_core.hpp", "core"),
        ]

        symbol, file, subdir = self.rng.choice(symbols)

        conv = AgenticConversation(category="lsp_navigation", complexity="medium")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(f"Where is `{symbol}` defined and what calls it?"))

        conv.turns.append(self._think(
            f"Using LSP to navigate to the definition and then get the call hierarchy."
        ))

        # Goto definition
        conv.turns.append(self._assistant("", tool_calls=[
            self._tool_call("goto_definition", {"symbol": symbol, "file": f"src/{subdir}/{file}"})
        ]))
        conv.turns.append(self._tool_result("goto_definition",
            f"Definition found:\n"
            f"  File: src/{subdir}/{file}\n"
            f"  Line: {self.rng.randint(50, 200)}\n"
            f"  Kind: method"
        ))

        # Call hierarchy
        conv.turns.append(self._assistant("Now getting the call hierarchy.", tool_calls=[
            self._tool_call("get_call_hierarchy", {"symbol": symbol, "file": f"src/{subdir}/{file}", "direction": "incoming"})
        ]))

        callers = [
            f"  src/{subdir}/main_loop.cpp:L{self.rng.randint(100,500)}",
            f"  src/cli/cli_handler.cpp:L{self.rng.randint(100,500)}",
            f"  src/agent/autonomy.cpp:L{self.rng.randint(100,500)}",
        ]
        conv.turns.append(self._tool_result("get_call_hierarchy",
            f"Incoming calls to {symbol}:\n" + "\n".join(callers)
        ))

        conv.turns.append(self._assistant(
            f"`{symbol}` is defined in `src/{subdir}/{file}`. "
            f"It's called from {len(callers)} locations:\n" +
            "\n".join([f"- {c.strip()}" for c in callers])
        ))

        conv.tools_used = ["think", "goto_definition", "get_call_hierarchy"]
        return conv


class KnowledgeGraphScenario(ScenarioGenerator):
    """Query and build the knowledge graph: decisions, relationships, patterns."""

    def generate(self) -> AgenticConversation:
        queries = [
            ("Why did we switch from std::function to raw function pointers?",
             "Convention", "std::function was removed for performance and binary size. Raw function pointers with void* userData provide equivalent flexibility without STL overhead."),
            ("What's the dependency graph for the hotpatch subsystem?",
             "relationships", "memory_hotpatch → byte_level_hotpatcher → server_hotpatch → unified_hotpatch_manager (coordinator)"),
            ("Show me the history of changes to the command registry",
             "history", "15 changes: initial X-macro system (Jan), added CmdFlags (Feb), async dispatch (Mar), agent tool integration (Apr)"),
        ]

        query, search_type, answer = self.rng.choice(queries)

        conv = AgenticConversation(category="knowledge_graph", complexity="medium")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user(query))

        conv.turns.append(self._assistant("Searching the knowledge graph.", tool_calls=[
            self._tool_call("knowledge_search", {"query": query, "type": "all", "top_k": 5})
        ]))
        conv.turns.append(self._tool_result("knowledge_search",
            f"Results from knowledge graph:\n"
            f"  Match 1 (confidence: 0.94): {answer}\n"
            f"  Match 2 (confidence: 0.72): Related pattern in commit history"
        ))

        conv.turns.append(self._assistant(answer))
        conv.tools_used = ["knowledge_search"]
        return conv


class SwarmDistributedScenario(ScenarioGenerator):
    """Distribute work across the agent swarm."""

    def generate(self) -> AgenticConversation:
        conv = AgenticConversation(category="swarm", complexity="expert")
        conv.turns.append(ConversationTurn(role="system", content=SYSTEM_PROMPT))
        conv.turns.append(self._user("Run a distributed security audit across the entire codebase"))

        conv.turns.append(self._think(
            "Security audit across the full codebase — this is a good candidate for swarm distribution. "
            "I'll distribute the analysis across available nodes for parallel processing."
        ))

        conv.turns.append(self._assistant("Distributing security audit across the swarm.", tool_calls=[
            self._tool_call("swarm_distribute_task", {
                "task_type": "analyze",
                "files": ["src/core/**", "src/server/**", "src/agent/**", "src/asm/**"],
                "gpu_required": False
            })
        ]))
        conv.turns.append(self._tool_result("swarm_distribute_task",
            "Task distributed to 4 nodes.\n"
            "Node 1: src/core/** (42 files) — completed in 12s\n"
            "Node 2: src/server/** (8 files) — completed in 3s\n"
            "Node 3: src/agent/** (15 files) — completed in 6s\n"
            "Node 4: src/asm/** (5 files) — completed in 2s\n\n"
            "Results:\n"
            "  HIGH: 1 — Unvalidated memcpy in byte_level_hotpatcher.cpp:L234\n"
            "  MEDIUM: 3 — Missing bounds checks in memory_patch.asm\n"
            "  LOW: 7 — Unused variable warnings"
        ))

        conv.turns.append(self._assistant(
            "Security audit complete (distributed across 4 nodes, took 12s total):\n\n"
            "**HIGH** (1): Unvalidated memcpy size in byte_level_hotpatcher.cpp — could cause buffer overflow\n"
            "**MEDIUM** (3): Missing bounds checks in memory_patch.asm — could write past buffer\n"
            "**LOW** (7): Unused variables\n\n"
            "Want me to auto-fix the HIGH and MEDIUM issues?"
        ))

        conv.tools_used = ["think", "swarm_distribute_task"]
        return conv


# ---------------------------------------------------------------------------
# Conversation serializer — converts to ChatML JSONL
# ---------------------------------------------------------------------------

def serialize_conversation(conv: AgenticConversation) -> str:
    """Convert an AgenticConversation to ChatML-format JSON for training."""
    messages = []

    for turn in conv.turns:
        msg = {"role": turn.role}

        if turn.role == "system":
            msg["content"] = turn.content

        elif turn.role == "user":
            msg["content"] = turn.content

        elif turn.role == "assistant":
            if turn.tool_calls:
                # Format tool calls as structured JSON within the content
                tool_call_text = ""
                for tc in turn.tool_calls:
                    tool_call_text += f"\n<tool_call>\n{json.dumps({'name': tc.name, 'arguments': tc.arguments}, indent=2)}\n</tool_call>\n"

                msg["content"] = (turn.content + tool_call_text).strip()
            else:
                msg["content"] = turn.content

        elif turn.role == "tool":
            if turn.tool_results:
                tr = turn.tool_results[0]
                msg["role"] = "tool"
                msg["name"] = tr.name
                msg["content"] = tr.output
            else:
                msg["content"] = ""

        if msg.get("content") or msg.get("role") == "system":
            messages.append(msg)

    record = {
        "messages": messages,
        "metadata": {
            "category": conv.category,
            "complexity": conv.complexity,
            "tools_used": conv.tools_used,
            "source": "agentic_synthetic"
        }
    }

    return json.dumps(record, ensure_ascii=False)


# ---------------------------------------------------------------------------
# Main generator
# ---------------------------------------------------------------------------

class AgenticTrainingDataGenerator:
    """Orchestrates generation of agentic tool-use training data."""

    SCENARIO_WEIGHTS = {
        "read_edit_verify": 0.20,
        "search_refactor": 0.12,
        "debug_crash": 0.10,
        "build_fix": 0.15,
        "hotpatch": 0.08,
        "autonomous_workflow": 0.08,
        "failure_recovery": 0.07,
        "multi_file_implement": 0.08,
        "terminal_build": 0.05,
        "lsp_navigation": 0.04,
        "knowledge_graph": 0.02,
        "swarm": 0.01,
    }

    def __init__(self, config: Dict[str, Any] = None):
        self.config = config or DEFAULT_CONFIG
        self.rng = random.Random(self.config.get("seed", 42))
        self.corpus = None  # Loaded lazily
        self.scenarios = {}
        self.stats = {
            "total_generated": 0,
            "by_category": {},
            "by_complexity": {"simple": 0, "medium": 0, "complex": 0, "expert": 0},
            "tools_histogram": {},
            "errors": 0,
        }

    def _init_corpus(self):
        if self.corpus is None:
            print("Phase 2.5: Agentic Tool-Use Training Data Generator")
            print("=" * 60)
            self.corpus = CorpusCache(self.config["raw_corpus"], max_entries=5000)

    def _init_scenarios(self):
        self.scenarios = {
            "read_edit_verify": ReadEditVerifyScenario(self.corpus, self.rng),
            "search_refactor": SearchAndRefactorScenario(self.corpus, self.rng),
            "debug_crash": DebugCrashScenario(self.corpus, self.rng),
            "build_fix": BuildFixLoopScenario(self.corpus, self.rng),
            "hotpatch": HotpatchScenario(self.corpus, self.rng),
            "autonomous_workflow": AutonomousWorkflowScenario(self.corpus, self.rng),
            "failure_recovery": FailureRecoveryScenario(self.corpus, self.rng),
            "multi_file_implement": MultiFileImplementScenario(self.corpus, self.rng),
            "terminal_build": TerminalAndBuildScenario(self.corpus, self.rng),
            "lsp_navigation": LSPNavigationScenario(self.corpus, self.rng),
            "knowledge_graph": KnowledgeGraphScenario(self.corpus, self.rng),
            "swarm": SwarmDistributedScenario(self.corpus, self.rng),
        }

    def _pick_scenario(self) -> str:
        categories = list(self.SCENARIO_WEIGHTS.keys())
        weights = [self.SCENARIO_WEIGHTS[c] for c in categories]
        return self.rng.choices(categories, weights=weights, k=1)[0]

    def generate(self):
        """Generate all agentic training data."""
        self._init_corpus()
        self._init_scenarios()

        output_path = self.config["agentic_jsonl"]
        max_samples = self.config.get("max_samples", 50000)

        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        print(f"\nGenerating {max_samples} agentic tool-use training samples...")
        print(f"Output: {output_path}")
        print(f"Scenarios: {len(self.scenarios)}")
        print()

        start_time = time.time()
        checkpoint_interval = 5000

        with open(output_path, "w", encoding="utf-8") as f:
            for i in range(max_samples):
                try:
                    category = self._pick_scenario()
                    scenario = self.scenarios[category]
                    conv = scenario.generate()

                    line = serialize_conversation(conv)
                    f.write(line + "\n")

                    # Update stats
                    self.stats["total_generated"] += 1
                    self.stats["by_category"][category] = self.stats["by_category"].get(category, 0) + 1
                    self.stats["by_complexity"][conv.complexity] += 1
                    for tool in conv.tools_used:
                        self.stats["tools_histogram"][tool] = self.stats["tools_histogram"].get(tool, 0) + 1

                    # Progress
                    if (i + 1) % 1000 == 0:
                        elapsed = time.time() - start_time
                        rate = (i + 1) / elapsed
                        eta = (max_samples - i - 1) / rate
                        print(f"  [{i+1}/{max_samples}] {rate:.0f} samples/sec, ETA: {eta:.0f}s")

                    # Checkpoint
                    if (i + 1) % checkpoint_interval == 0:
                        f.flush()

                except Exception as e:
                    self.stats["errors"] += 1
                    if self.stats["errors"] < 10:
                        print(f"  [ERROR] Sample {i}: {e}")
                    continue

        elapsed = time.time() - start_time
        self._print_stats(elapsed)

        return output_path

    def _print_stats(self, elapsed: float):
        print(f"\n{'=' * 60}")
        print(f"Agentic Training Data Generation Complete!")
        print(f"{'=' * 60}")
        print(f"Total samples:  {self.stats['total_generated']}")
        print(f"Errors:         {self.stats['errors']}")
        print(f"Time:           {elapsed:.1f}s ({self.stats['total_generated']/elapsed:.0f} samples/sec)")
        print(f"\nBy category:")
        for cat, count in sorted(self.stats["by_category"].items(), key=lambda x: -x[1]):
            pct = count / self.stats["total_generated"] * 100
            print(f"  {cat:30s} {count:6d} ({pct:.1f}%)")
        print(f"\nBy complexity:")
        for comp, count in sorted(self.stats["by_complexity"].items(), key=lambda x: -x[1]):
            pct = count / self.stats["total_generated"] * 100
            print(f"  {comp:30s} {count:6d} ({pct:.1f}%)")
        print(f"\nTool usage (top 15):")
        for tool, count in sorted(self.stats["tools_histogram"].items(), key=lambda x: -x[1])[:15]:
            print(f"  {tool:30s} {count:6d}")
        print()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    config_path = os.path.join(os.path.dirname(__file__), "pipeline_config.json")

    config = DEFAULT_CONFIG.copy()
    if os.path.exists(config_path):
        with open(config_path, "r") as f:
            pipe_cfg = json.load(f)
        corpus_cfg = pipe_cfg.get("corpus", {})
        config["raw_corpus"] = corpus_cfg.get("raw_jsonl", config["raw_corpus"])
        config["instructions_jsonl"] = corpus_cfg.get("instructions_jsonl", config["instructions_jsonl"])
        config["agentic_jsonl"] = corpus_cfg.get("output_dir", "F:\\RawrXD-AI-Training\\corpus") + "\\agentic_tool_use.jsonl"
        config["corpus_dir"] = corpus_cfg.get("output_dir", config["corpus_dir"])

    # CLI overrides
    for arg in sys.argv[1:]:
        if arg.startswith("--max-samples="):
            config["max_samples"] = int(arg.split("=")[1])
        elif arg.startswith("--seed="):
            config["seed"] = int(arg.split("=")[1])
        elif arg.startswith("--output="):
            config["agentic_jsonl"] = arg.split("=")[1]

    generator = AgenticTrainingDataGenerator(config)
    output = generator.generate()
    print(f"Output written to: {output}")

    # Also merge into the main instructions.jsonl so Phase 3 sees everything
    instructions_path = config["instructions_jsonl"]
    if os.path.exists(instructions_path):
        print(f"\nMerging agentic data into {instructions_path}...")
        with open(output, "r", encoding="utf-8") as src:
            agentic_lines = src.readlines()
        with open(instructions_path, "a", encoding="utf-8") as dst:
            dst.writelines(agentic_lines)
        total_lines = sum(1 for _ in open(instructions_path, "r", encoding="utf-8"))
        print(f"Merged. Total training samples: {total_lines}")
    else:
        print(f"[WARN] {instructions_path} not found — agentic data saved standalone.")

    print("\nPhase 2.5 complete. Ready for Phase 3 (QLoRA fine-tuning).")


if __name__ == "__main__":
    main()
