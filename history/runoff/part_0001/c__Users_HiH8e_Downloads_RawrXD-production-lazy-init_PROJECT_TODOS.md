# RawrXD Pure MASM IDE - PROJECT TODO LIST
**Date**: December 29, 2025  
**Status**: Active Development  
**Phase**: 4 (Settings Dialog) + Extended Features

---

## 📋 PRIORITY TIERS

### 🔴 TIER 1: CRITICAL (Blocks Phase 4 Completion)

#### Settings Dialog (qt6_settings_dialog.asm)
- [ ] **Line 698-707**: LoadSettingsToUI - Implement control value loading for all tabs
  - [ ] Load General tab checkboxes (auto_save, startup_fullscreen)
  - [ ] Load font_size spinner
  - [ ] Load Model tab fields (path, default model)
  - [ ] Load Chat tab fields (temperature, max_tokens, system_prompt)
  - [ ] Load Security tab fields (API key, encryption, secure_storage)

- [ ] **Line 732-741**: SaveSettingsFromUI - Implement control value saving
  - [ ] Save General tab checkboxes
  - [ ] Save font_size value
  - [ ] Save Model tab paths and selections
  - [ ] Save Chat tab numeric and text fields
  - [ ] Save Security tab settings

- [ ] **Line 766**: OnTabSelectionChanged - Implement tab content switching
  - [ ] Hide inactive tab pages
  - [ ] Show active tab page with ShowWindow()
  - [ ] Update UI state on tab change

- [ ] **Line 605, 624, 643**: Training/CI/CD/Enterprise Tab Controls
  - [ ] Implement CreateTrainingTabControls with advanced options
  - [ ] Implement CreateCICDTabControls with pipeline settings
  - [ ] Implement CreateEnterpriseTabControls with compliance options

- [ ] **Line 1278**: HandleControlChange - Implement control change handlers
  - [ ] Set dirty flag on any control change
  - [ ] Validate input constraints
  - [ ] Enable/disable Apply button conditionally

#### Registry Persistence (registry_persistence.asm)
- [ ] Implement LoadSettingsFromRegistry complete function
  - [ ] Open HKEY_CURRENT_USER registry path
  - [ ] Read all 12+ settings values (DWORD and SZ types)
  - [ ] Apply default values if registry keys don't exist
  - [ ] Handle registry read errors gracefully

- [ ] Implement SaveSettingsToRegistry complete function
  - [ ] Create/open registry paths for each settings category
  - [ ] Write all SETTINGS_DATA fields to registry
  - [ ] Handle registry write errors
  - [ ] Verify atomic writes

#### Control Creation Functions (qt6_settings_dialog.asm)
- [ ] **CreateCheckbox**: Full Windows API CreateWindowExW implementation
- [ ] **CreateButton**: Complete button creation with proper styles
- [ ] **CreateEditControl**: Multiline edit support with ES_MULTILINE
- [ ] **CreateStaticText**: Label creation with proper alignment
- [ ] **CreateDropdown**: ComboBox with CBS_DROPDOWNLIST style

---

### 🟠 TIER 2: HIGH PRIORITY (Phase 4 Completion)

#### Chat Persistence (chat_persistence.asm)
- [ ] **Line 137**: FormatJsonHeader - Implement JSON header formatting
  - [ ] Format version, timestamp, session_id fields
  - [ ] Allocate buffer dynamically
  - [ ] Handle sprintf-like formatting for JSON syntax

- [ ] **Line 251**: ImplementHexFormatting - JSON hex value formatting
  - [ ] Convert binary data to hex string
  - [ ] Format as JSON escaped string
  - [ ] Handle encoding/decoding

- [ ] **Line 291**: ExtractMessageFields - Parse message structures
  - [ ] Extract role (user/assistant)
  - [ ] Parse timestamp
  - [ ] Extract content text

- [ ] **Line 377**: ImplementConversion - Full persistence conversion
  - [ ] Serialize settings to JSON format
  - [ ] Deserialize from JSON files
  - [ ] Handle version migration

#### Status Bar (qt6_statusbar.asm)
- [ ] **Line 186-194**: CreateStatusBar implementation
  - [ ] Allocate STATUS_BAR structure with text buffers
  - [ ] Initialize all 5-7 segments (mode, info, progress, etc.)
  - [ ] Create GDI font (Segoe UI, 10pt)
  - [ ] Create background and text brushes
  - [ ] Set proper window flags and dimensions

- [ ] **Line 210-211**: Cleanup in DestroyStatusBar
  - [ ] Delete font handle via DeleteObject
  - [ ] Delete brush handles
  - [ ] Free segment text buffers
  - [ ] Free main STATUS_BAR structure

#### Theme System (masm_theme_system_complete.asm)
- [ ] **Line 389**: HighContrastTheme implementation
  - [ ] Define high-contrast color palette
  - [ ] Implement accessible colors (WCAG compliance)
  - [ ] Add contrast ratio validation

- [ ] **Line 787**: ExportThemeToFile - Theme serialization
  - [ ] Format theme as JSON-like structure
  - [ ] Write to file with proper encoding
  - [ ] Include version info

- [ ] **Line 799**: ImportThemeFromFile - Theme deserialization
  - [ ] Read theme file with error handling
  - [ ] Parse JSON-like format
  - [ ] Validate theme structure
  - [ ] Apply loaded theme

---

### 🟡 TIER 3: MEDIUM PRIORITY (Extended Features)

#### File Tree Navigation (file_tree_context_menu.asm)
- [ ] **Line 231**: TreeTraversal - Walk directory tree recursively
  - [ ] Implement depth-first tree walk
  - [ ] Build parent-child relationships
  - [ ] Handle circular references

- [ ] **Line 377**: PathRelativization - Convert absolute to relative paths
  - [ ] Strip base path from file paths
  - [ ] Handle different drive letters
  - [ ] Preserve path separators

- [ ] **Line 419**: ArgumentBuilding - Construct command-line arguments
  - [ ] Quote paths with spaces
  - [ ] Escape special characters
  - [ ] Build proper command strings

- [ ] **Line 431**: TreeItemRefresh - Refresh UI after file operations
  - [ ] Invalidate tree items
  - [ ] Redraw affected branches
  - [ ] Update selection state

#### GUI Builder (masm_visual_gui_builder.asm)
- [ ] **Line 747**: CreateWindowExA calls for all widgets
  - [ ] Map widget descriptors to Win32 class names
  - [ ] Create proper window hierarchies
  - [ ] Set correct styles and extended styles
  - [ ] Link widgets with parent containers

#### Menu System (menu_system.asm)
- [ ] **Line 554**: "Create New File" implementation
- [ ] **Line 559**: "Open File Dialog" integration
- [ ] **Line 564**: "Save Current File" handler
- [ ] **Line 569**: "Close Application" with cleanup
- [ ] **Line 574**: "Toggle Theme" dynamic switching

#### Output Pane (output_pane_search.asm)
- [ ] **Line 261**: Case-insensitive search matching
  - [ ] Implement CharUpperA for comparison
  - [ ] Highlight matches regardless of case
  - [ ] Preserve original text casing in display

#### Pipeline Executor (pipeline_executor_complete.asm)
- [ ] **Line 561**: Process spawning with command execution
  - [ ] Use CreateProcessA with proper arguments
  - [ ] Capture stdout/stderr
  - [ ] Handle process termination

#### Agent Learning System (agent_learning_system.asm)
- [ ] **Line 435**: Call output_log_agent or logging system
  - [ ] Wire up logging callbacks
  - [ ] Implement structured logging
  - [ ] Add timestamp/context info

#### Phase 2 Integration (phase2_integration.asm)
- [ ] **Line 438**: Open file in editor (Phase 3 integration)
  - [ ] Send file path to editor component
  - [ ] Activate editor window
  - [ ] Position cursor to specific line

---

### 🔵 TIER 4: NICE TO HAVE (Future Enhancements)

#### String Formatting (asm_string.asm)
- [ ] **Line 830**: Full sprintf implementation
  - [ ] Support %d, %s, %x format codes
  - [ ] Handle width and precision modifiers
  - [ ] Buffer overflow protection

#### Memory Management (asm_memory.asm)
- [ ] **Line 493**: Custom memory allocator with tracking
  - [ ] Implement heap profiling
  - [ ] Add leak detection
  - [ ] Support custom alignment

#### Synchronization (asm_sync.asm/asm_sync_temp.asm)
- [ ] **Lines 494-557**: Replace placeholder Win32 API wrappers
  - [ ] InitializeCriticalSection wrapper
  - [ ] EnterCriticalSection wrapper
  - [ ] LeaveCriticalSection wrapper
  - [ ] DeleteCriticalSection wrapper
  - [ ] CreateEventExW wrapper
  - [ ] SetEvent wrapper
  - [ ] ResetEvent wrapper
  - [ ] WaitForSingleObject wrapper

#### Events System (asm_events.asm)
- [ ] **Line 109**: Event handler registry
  - [ ] Use dynamic array or hash map
  - [ ] Support multiple handlers per event
  - [ ] Implement handler removal

#### Ghost Text Suggestions (ghost_text_suggestions.asm)
- [ ] **Line 539**: Render ghost text overlay
  - [ ] Draw text with reduced opacity
  - [ ] Position after cursor
  - [ ] Update on keystroke

- [ ] **Line 549**: Text insertion logic
  - [ ] Accept suggestion with Tab/Ctrl+Right
  - [ ] Insert at cursor position
  - [ ] Update editor content

#### BPE Tokenizer (cpp_to_masm_bpe_tokenizer.asm)
- [ ] **Line 492**: Full placeholder implementation
  - [ ] Implement byte-pair encoding
  - [ ] Support vocabulary loading
  - [ ] Handle token merging

#### GUI Main Loop (masm_gui_main.asm)
- [ ] **Line 422**: Panel resizing logic
  - [ ] Handle WM_SIZE messages
  - [ ] Recalculate panel layout
  - [ ] Notify child windows of size changes

#### AI Orchestration (ai_orchestration_glue.asm)
- [ ] **Line 34**: Replace placeholder polling
  - [ ] Implement event loop
  - [ ] Check for pending messages
  - [ ] Process queued events

- [ ] **Line 41**: Replace placeholder shutdown
  - [ ] Clean up resources
  - [ ] Close event handles
  - [ ] Unload components

---

## 📊 COMPLETION METRICS

### By File
| File | TODOs | Priority | Status |
|------|-------|----------|--------|
| qt6_settings_dialog.asm | 20+ | CRITICAL | 🔴 |
| registry_persistence.asm | 2 | CRITICAL | 🔴 |
| chat_persistence.asm | 4 | HIGH | 🟠 |
| qt6_statusbar.asm | 8 | HIGH | 🟠 |
| masm_theme_system_complete.asm | 3 | HIGH | 🟠 |
| file_tree_context_menu.asm | 4 | MEDIUM | 🟡 |
| masm_visual_gui_builder.asm | 1 | MEDIUM | 🟡 |
| menu_system.asm | 5 | MEDIUM | 🟡 |
| output_pane_search.asm | 1 | MEDIUM | 🟡 |
| pipeline_executor_complete.asm | 1 | MEDIUM | 🟡 |
| agent_learning_system.asm | 1 | MEDIUM | 🟡 |
| phase2_integration.asm | 1 | MEDIUM | 🟡 |
| **TOTAL** | **51+** | Mixed | In Progress |

### By Priority
- 🔴 CRITICAL: 22+ items (Phase 4 blocking)
- 🟠 HIGH: 15+ items (Extended UI)
- 🟡 MEDIUM: 10+ items (Navigation/Commands)
- 🔵 NICE: 14+ items (Future work)

---

## 🎯 NEXT STEPS

### Immediate (This Session)
1. [ ] Complete qt6_settings_dialog.asm control functions
2. [ ] Implement registry_persistence.asm save/load
3. [ ] Test Phase 4 integration

### Short Term (1-2 Days)
1. [ ] Implement chat_persistence.asm JSON functions
2. [ ] Complete status bar implementation
3. [ ] Add theme system export/import

### Medium Term (1 Week)
1. [ ] File tree navigation
2. [ ] GUI widget creation
3. [ ] Menu command handlers

### Long Term (2+ Weeks)
1. [ ] String formatting engine
2. [ ] Memory profiling
3. [ ] Win32 API wrapper library
4. [ ] Advanced control rendering

---

## 📝 NOTES

- All TODOs are marked with `; TODO:` in source files
- Placeholders are marked with `; Placeholder` or `; MVP:`
- Use Ctrl+Shift+F to find all TODOs in workspace
- Each section estimated 2-4 hours to complete
- Total estimated work: 40-50 hours for all items
- Critical path items required for Phase 4 completion

---

**Last Updated**: December 29, 2025  
**Assigned**: RawrXD Development Team  
**Repository**: c:/Users/HiH8e/Downloads/RawrXD-production-lazy-init/src/masm/final-ide/
