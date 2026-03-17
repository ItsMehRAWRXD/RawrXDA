# Win32 IDE Testing Checklist

**Version:** 1.0.0  
**Date:** December 18, 2025

This document provides a comprehensive manual testing checklist for the Win32 IDE. Use this to verify all features before release.

---

## Pre-Flight Checks

- [ ] Visual C++ Redistributable 2022 (x64) installed
- [ ] Node.js v18+ installed and in PATH
- [ ] `OPENAI_API_KEY` environment variable set
- [ ] Build completed successfully: `AgenticIDEWin.exe` exists in `build-win32-only/bin/Release/`
- [ ] No compile errors or critical warnings

---

## 1. Startup & Initialization

### Test: Cold Start
- [ ] Launch `AgenticIDEWin.exe` from File Explorer
- [ ] IDE window appears within 3 seconds
- [ ] No crash dialogs or error messages
- [ ] Check `logs/startup.log` for clean initialization

### Test: Warm Start
- [ ] Close IDE
- [ ] Launch again
- [ ] IDE appears within 1 second (cached)
- [ ] Previous session restored (if enabled)

### Test: Command Line Launch
- [ ] Open PowerShell
- [ ] Run: `.\AgenticIDEWin.exe "C:\TestWorkspace"`
- [ ] IDE opens with TestWorkspace as root
- [ ] File tree shows TestWorkspace contents

---

## 2. File Tree Operations

### Test: Load Workspace
- [ ] File → Open Folder → Select directory with 100+ files
- [ ] File tree populates within 2 seconds
- [ ] Folders show expand/collapse arrows
- [ ] Files have correct icons (via Shell API)

### Test: Navigation
- [ ] Click folder to expand
- [ ] Click again to collapse
- [ ] Double-click file → Opens in editor
- [ ] Select multiple files (Ctrl+Click) → All selected

### Test: Context Menu
- [ ] Right-click file → Context menu appears
- [ ] Menu items: Open, Rename, Delete, Properties, Refresh
- [ ] Click "Rename" → File renames correctly
- [ ] Click "Delete" → Confirmation prompt → File deleted
- [ ] Click "Refresh" → Tree updates with changes

### Test: Large Directory
- [ ] Navigate to `C:\Windows\System32`
- [ ] File tree loads without hang
- [ ] Scrolling is smooth
- [ ] No memory spikes (Task Manager)

### Test: Drag and Drop
- [ ] Drag file from File Explorer to file tree
- [ ] File copies to workspace
- [ ] Tree refreshes to show new file

---

## 3. Editor Operations

### Test: Open File
- [ ] Double-click file in tree → Editor tab opens
- [ ] File content displays correctly
- [ ] Syntax highlighting applied (if supported)
- [ ] Cursor positioned at start of file

### Test: Edit and Save
- [ ] Type text in editor
- [ ] Tab shows modified indicator (asterisk or dot)
- [ ] Ctrl+S → File saved
- [ ] Modified indicator clears
- [ ] Check file on disk → Changes persisted

### Test: Multiple Tabs
- [ ] Open 10 files → 10 tabs appear
- [ ] Click each tab → Correct content displays
- [ ] Ctrl+Tab → Cycles through tabs
- [ ] Close tab (X button) → Tab closes
- [ ] Close unsaved tab → Confirmation prompt

### Test: Undo/Redo
- [ ] Type text
- [ ] Ctrl+Z → Text undone
- [ ] Ctrl+Y → Text redone
- [ ] Undo stack maintains 100+ operations

### Test: Large File
- [ ] Open file > 1 MB
- [ ] File loads within 500ms
- [ ] Scrolling is smooth
- [ ] Editing is responsive

### Test: Encoding
- [ ] Save file with non-ASCII characters (UTF-8)
- [ ] Close and reopen file
- [ ] Characters display correctly

---

## 4. Terminal Operations

### Test: Spawn Terminal
- [ ] Click Terminal panel
- [ ] PowerShell/CMD spawns within 500ms
- [ ] Prompt appears
- [ ] Type `echo hello` → Output displays

### Test: Multiple Terminals
- [ ] Spawn second terminal (New Terminal button)
- [ ] Two terminals independent
- [ ] Switch between terminals → Correct output shows
- [ ] Commands in one terminal don't affect other

### Test: Long Output
- [ ] Run: `Get-ChildItem C:\Windows\System32 -Recurse`
- [ ] Output streams without UI freeze
- [ ] Scrollback works (up to 10,000 lines)
- [ ] No crash or memory leak

### Test: Long-Running Command
- [ ] Run: `ping 8.8.8.8 -t`
- [ ] Output streams continuously
- [ ] UI remains responsive
- [ ] Ctrl+C → Command stops

### Test: Working Directory Persistence
- [ ] Run: `cd C:\Temp`
- [ ] Run: `pwd` → Shows `C:\Temp`
- [ ] Run another command → Still in `C:\Temp`

---

## 5. AI Chat & Orchestration

### Test: Chat Basics
- [ ] Click Chat panel
- [ ] Type message in input box
- [ ] Press Enter or click Send
- [ ] Response streams in transcript
- [ ] No UI freeze during streaming

### Test: Slash Commands
- [ ] Type `/cursor write a hello world function`
- [ ] Cursor agent executes
- [ ] Code appears in editor
- [ ] Check `logs/chat.log` for request/response

### Test: Paint Generation
- [ ] Type `/paintgen a sunset over mountains`
- [ ] Image generation request sent
- [ ] Paint window opens with generated image (within 30s)
- [ ] Image is high quality

### Test: Orchestration
- [ ] Switch to Orchestra panel
- [ ] Select model from dropdown
- [ ] Enter objective: "Analyze this file and suggest improvements"
- [ ] Click Run
- [ ] Plan displays in log
- [ ] Code analysis appears
- [ ] Results inserted into editor

### Test: Multiple Chat Tabs
- [ ] Open new chat tab
- [ ] Type different messages in each tab
- [ ] Switch between tabs → Correct history shows
- [ ] Close tab → No crash

---

## 6. LSP Integration

### Test: Hover Tooltips
- [ ] Open C++ file
- [ ] Hover over function name
- [ ] Tooltip appears within 200ms
- [ ] Tooltip shows type/signature

### Test: Go to Definition
- [ ] Right-click function call
- [ ] Select "Go to Definition"
- [ ] Editor jumps to function definition
- [ ] Correct file opens if external

### Test: Diagnostics
- [ ] Introduce syntax error (e.g., missing semicolon)
- [ ] Save file
- [ ] Error underline appears
- [ ] Diagnostics panel shows error message

### Test: Document Symbols
- [ ] Open file with multiple functions
- [ ] View → Document Outline
- [ ] Symbol list appears
- [ ] Click symbol → Editor jumps to location

---

## 7. Paint Operations

### Test: Drawing Tools
- [ ] Click Paint panel
- [ ] Select Pencil tool
- [ ] Draw on canvas → Line appears
- [ ] Select Brush tool → Thicker line
- [ ] Select Eraser tool → Erase previous drawing
- [ ] Select Shape tools (Rectangle, Circle) → Shapes drawn

### Test: Undo/Redo
- [ ] Draw several strokes
- [ ] Click Undo → Last stroke removed
- [ ] Click Undo multiple times → Strokes removed in reverse order
- [ ] Click Redo → Strokes reappear

### Test: Color Picker
- [ ] Click foreground color button
- [ ] Color picker appears
- [ ] Select red → Foreground changes to red
- [ ] Draw → Red stroke appears

### Test: Export
- [ ] File → Export as PNG
- [ ] Save dialog appears
- [ ] Enter filename → Save
- [ ] Open saved PNG → Image matches canvas

### Test: AI Generation Integration
- [ ] Type `/paintgen abstract art` in chat
- [ ] Generated image loads in paint window
- [ ] Image is editable with tools
- [ ] Export works

---

## 8. Metrics & Logging

### Test: Log Files Created
- [ ] Navigate to `%LOCALAPPDATA%\RawrXD\logs\`
- [ ] Verify files exist: `startup.log`, `gui.log`, `chat.log`, etc.
- [ ] Open `startup.log` → Contains initialization messages
- [ ] No ERROR or CRITICAL entries (unless expected)

### Test: Audit Trail
- [ ] Perform actions (open file, send chat, run command)
- [ ] Open `logs/audit.log`
- [ ] Verify actions logged with timestamps
- [ ] Entries are immutable (no modifications possible)

### Test: Metrics Collection
- [ ] Perform various actions for 5 minutes
- [ ] View → Performance Dashboard (if implemented)
- [ ] Metrics displayed (token throughput, latency, etc.)
- [ ] No performance degradation from metrics

---

## 9. Enterprise Features

### Test: Multi-Tenant Isolation
- [ ] Set tenant ID in config: `"tenantId": "test-tenant-1"`
- [ ] Perform actions
- [ ] Check logs → Tenant ID appears in entries
- [ ] Switch tenant → New tenant's data isolated

### Test: Rate Limiting
- [ ] Configure rate limit: `"requestsPerMinute": 5`
- [ ] Send 10 AI requests rapidly
- [ ] Verify 5 succeed, rest rate-limited
- [ ] Wait 1 minute → Quota resets

### Test: Cache Layer
- [ ] Send AI request: "What is 2+2?"
- [ ] Send same request again
- [ ] Second request returns instantly (cache hit)
- [ ] Check logs → Cache hit logged

---

## 10. Voice Processing

### Test: Text-to-Speech
- [ ] Type text in chat or editor
- [ ] Click "Speak" button (or voice command)
- [ ] Windows SAPI speaks text within 1 second
- [ ] Audio is clear and intelligible

### Test: Accent Selection
- [ ] Settings → Voice → Accent → Select "British"
- [ ] Speak text again
- [ ] British accent used

---

## 11. Stress Testing

### Test: Many Open Tabs
- [ ] Open 50+ files in editor
- [ ] Switch between tabs → No lag
- [ ] Memory usage acceptable (< 1 GB)
- [ ] Close all tabs → Memory released

### Test: Long Session
- [ ] Run IDE for 8+ hours
- [ ] Perform various actions periodically
- [ ] No memory leaks (check Task Manager)
- [ ] No crashes or slowdowns

### Test: Large Workspace
- [ ] Open workspace with 10,000+ files
- [ ] File tree loads (may take longer)
- [ ] Search and navigation still functional
- [ ] No excessive memory usage

---

## 12. Error Recovery

### Test: Bridge Crash
- [ ] Kill Node.js orchestration bridge process
- [ ] Send AI request
- [ ] IDE attempts reconnect
- [ ] Bridge respawns within 5 seconds
- [ ] Request completes successfully

### Test: LSP Server Crash
- [ ] Kill LSP server process (e.g., clangd)
- [ ] Hover over code
- [ ] No tooltip (expected)
- [ ] IDE does not crash
- [ ] LSP respawns on next file open

### Test: Out of Disk Space
- [ ] Fill disk to < 10 MB free
- [ ] Try to save file
- [ ] Error message displays
- [ ] IDE does not crash
- [ ] Unsaved changes not lost

---

## 13. Security

### Test: API Key Protection
- [ ] Open config files and logs
- [ ] Verify `OPENAI_API_KEY` never logged in plaintext
- [ ] Key stored in environment variable only

### Test: File Permissions
- [ ] Try to open file without read permissions
- [ ] Error message displays
- [ ] IDE does not crash

### Test: Malicious Input
- [ ] Type special characters in chat: `<script>alert('XSS')</script>`
- [ ] Verify no script execution
- [ ] Text displayed as-is (sanitized)

---

## 14. Accessibility

### Test: Keyboard Navigation
- [ ] Navigate UI using Tab key
- [ ] All controls reachable via keyboard
- [ ] Enter/Space activates buttons
- [ ] Escape closes dialogs

### Test: Screen Reader Compatibility
- [ ] Enable Windows Narrator
- [ ] Navigate UI
- [ ] Controls announced correctly
- [ ] Text content readable

---

## 15. Performance Benchmarks

### Startup Time
- [ ] Cold start < 3 seconds
- [ ] Warm start < 1 second

### File Operations
- [ ] Open file (1 MB) < 200ms
- [ ] Save file (1 MB) < 100ms
- [ ] Search workspace (1000 files) < 5 seconds

### UI Responsiveness
- [ ] Tab switch < 50ms
- [ ] Typing latency < 16ms (60 FPS)
- [ ] Scroll frame rate 60 FPS

### Terminal
- [ ] Command spawn < 500ms
- [ ] Output streaming latency < 100ms

### AI
- [ ] Chat response (first token) < 2 seconds
- [ ] Orchestrator plan generation < 5 seconds
- [ ] Image generation < 30 seconds

---

## 16. Regression Tests

Run after any code changes to ensure no breakage:

- [ ] File tree test harness: `.\file_tree_test.exe` → All tests pass
- [ ] Basic workflow: Open workspace → Edit file → Save → Run terminal command → Send chat → All succeed
- [ ] No new errors in logs after running workflow

---

## Sign-Off

**Tester Name:** ______________________  
**Date:** ______________________  
**Version Tested:** ______________________  
**Result:** ☐ Pass ☐ Fail ☐ Pass with Minor Issues

**Notes:**
```
[Add any issues found, observations, or recommendations]
```

---

**Last Updated:** December 18, 2025  
**Document Version:** 1.0  
**Status:** Complete ✅
