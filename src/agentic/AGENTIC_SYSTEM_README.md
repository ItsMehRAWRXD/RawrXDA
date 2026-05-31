# RawrXD Agentic System — Integration Guide

## Overview

The Agentic System transforms RawrXD from a passive editor into an **autonomous IDE** that can:
- **Observe** code patterns and detect issues
- **Suggest** improvements and fixes
- **Execute** safe actions automatically (in Autonomous mode)
- **Learn** from user approvals/rejections

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Win32IDE                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  Editor     │  │ Status Bar  │  │  Suggestion Panel   │ │
│  │  (files)    │  │ (mode indicator)│  (approve/reject)  │ │
│  └──────┬──────┘  └──────┬──────┘  └─────────────────────┘ │
│         │                │                                   │
│  ┌──────▼────────────────▼──────────────────────────────┐ │
│  │         AgenticIDEIntegration                          │ │
│  │  (central coordinator)                                  │ │
│  └──────┬────────────────┬──────────────────────────────┘ │
│         │                │                                   │
│  ┌──────▼──────┐  ┌──────▼──────┐                          │
│  │   Router    │  │     UI      │                          │
│  │  (brain)    │  │  (notifications)                       │
│  └──────┬──────┘  └─────────────┘                          │
│         │                                                   │
│  ┌──────▼──────────────────────────────────────────────┐   │
│  │              UnifiedInferenceRouter                     │   │
│  │         (all inference requests route here)            │   │
│  └───────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Components

### 1. AgenticRouterBridge (`AgenticRouterBridge.h/cpp`)
- **Purpose**: Central decision engine for agentic behavior
- **Modes**:
  - `Passive`: Only respond to explicit requests
  - `Suggestive`: Propose changes, require approval
  - `Autonomous`: Execute safe actions automatically
- **Features**:
  - Background observation thread (analyzes code every 5 seconds)
  - Pattern detection (unused includes, missing error handling, memory leaks)
  - TODO/FIXME comment tracking
  - Action queue with approval/rejection

### 2. AgenticUIBridge (`AgenticUIBridge.h/cpp`)
- **Purpose**: UI layer for notifications and suggestion panel
- **Features**:
  - Notification popups (auto-dismissing)
  - Suggestion panel with approve/reject buttons
  - Status bar integration
  - Mode toggle support

### 3. AgenticIDEIntegration (`AgenticIDEIntegration.h/cpp`)
- **Purpose**: Single integration point for Win32IDE
- **Usage**: One member variable, one initialize call

## Quick Start

### Step 1: Add Member to Win32IDE

```cpp
// In Win32IDE.h
#include "agentic/AgenticIDEIntegration.h"

class Win32IDE {
    // ... existing members ...
    std::unique_ptr<RawrXD::Agentic::AgenticIDEIntegration> m_agenticIntegration;
};
```

### Step 2: Initialize in Create()

```cpp
bool Win32IDE::Create(const std::wstring& title, int width, int height) {
    // ... existing creation code ...
    
    // Initialize agentic system
    m_agenticIntegration = std::make_unique<RawrXD::Agentic::AgenticIDEIntegration>();
    m_agenticIntegration->initialize(this, m_hInstance);
    
    // Set up callbacks
    m_agenticIntegration->onStatusUpdate([this](const std::string& status) {
        SetWindowTextA(m_hwndStatusBar, status.c_str());
    });
    
    m_agenticIntegration->onSuggestion([this](const RawrXD::Agentic::AgenticSuggestion& sug) {
        // Flash taskbar or show subtle indicator
        FlashWindow(m_hwndMain, TRUE);
    });
    
    return true;
}
```

### Step 3: Add Menu Items

```cpp
void Win32IDE::CreateMainMenu(HWND hWnd) {
    // ... existing menus ...
    
    HMENU hAgenticMenu = CreatePopupMenu();
    AppendMenuW(hAgenticMenu, MF_STRING, IDM_AGENTIC_TOGGLE, L"Toggle Autopilot Mode\tCtrl+Shift+A");
    AppendMenuW(hAgenticMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgenticMenu, MF_STRING | MF_CHECKED, IDM_AGENTIC_PASSIVE, L"Passive Mode");
    AppendMenuW(hAgenticMenu, MF_STRING, IDM_AGENTIC_SUGGESTIVE, L"Suggestive Mode");
    AppendMenuW(hAgenticMenu, MF_STRING, IDM_AGENTIC_AUTONOMOUS, L"Autonomous Mode");
    AppendMenuW(hAgenticMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgenticMenu, MF_STRING, IDM_SHOW_SUGGESTIONS, L"Show Suggestions\tCtrl+Shift+S");
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hAgenticMenu, L"Agentic");
}
```

### Step 4: Handle Commands

```cpp
void Win32IDE::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        // ... existing commands ...
        
        case IDM_AGENTIC_TOGGLE:
            m_agenticIntegration->toggleAutopilotMode();
            updateAgenticMenuChecks();
            break;
            
        case IDM_AGENTIC_PASSIVE:
            m_agenticIntegration->setAutopilotMode(RawrXD::Agentic::AgenticMode::Passive);
            updateAgenticMenuChecks();
            break;
            
        case IDM_AGENTIC_SUGGESTIVE:
            m_agenticIntegration->setAutopilotMode(RawrXD::Agentic::AgenticMode::Suggestive);
            updateAgenticMenuChecks();
            break;
            
        case IDM_AGENTIC_AUTONOMOUS:
            m_agenticIntegration->setAutopilotMode(RawrXD::Agentic::AgenticMode::Autonomous);
            updateAgenticMenuChecks();
            break;
            
        case IDM_SHOW_SUGGESTIONS:
            m_agenticIntegration->toggleSuggestionPanel();
            break;
    }
}
```

### Step 5: Update Status Bar

```cpp
void Win32IDE::updateStatusBar() {
    // ... existing status ...
    
    if (m_agenticIntegration && m_agenticIntegration->isInitialized()) {
        std::string agenticStatus = "Agentic: ";
        agenticStatus += m_agenticIntegration->getAutopilotModeString();
        
        int pending = m_agenticIntegration->getPendingSuggestionCount();
        if (pending > 0) {
            agenticStatus += " [" + std::to_string(pending) + " pending]";
        }
        
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, SB_PART_AGENTIC, (LPARAM)agenticStatus.c_str());
    }
}
```

### Step 6: Analyze Files on Open

```cpp
void Win32IDE::OpenFile(const std::string& path) {
    // ... existing open logic ...
    
    if (m_agenticIntegration) {
        m_agenticIntegration->analyzeCurrentFile(path);
    }
}
```

## Mode Behavior

### Passive Mode (Default)
- IDE behaves like a normal editor
- No automatic analysis or suggestions
- Agentic features only activate on explicit request

### Suggestive Mode
- IDE analyzes code in background
- Detects issues and improvements
- Shows suggestions in panel and notifications
- **Requires user approval** for all actions
- Best for: Learning the system's capabilities

### Autonomous Mode
- IDE analyzes code continuously
- Automatically executes **safe** actions (risk < 0.3, confidence > 0.8)
- Shows notifications for completed actions
- Queues risky actions for approval
- Best for: Experienced users who trust the system

## Detected Patterns

The system currently detects:

1. **Unused Includes**
   - Detects: `#include <header>` that appears unused
   - Suggestion: Remove include
   - Risk: 0.1 (very safe)
   - Auto-executable: Yes

2. **Missing Error Handling**
   - Detects: `fopen()` without null check
   - Suggestion: Add error handling
   - Risk: 0.15 (safe)
   - Auto-executable: No (requires review)

3. **Memory Leaks**
   - Detects: More `new` than `delete`
   - Suggestion: Use smart pointers
   - Risk: 0.25 (moderate)
   - Auto-executable: No

4. **TODO/FIXME Comments**
   - Detects: `TODO:`, `FIXME:`, `XXX:`, `HACK:`
   - Suggestion: Track as tasks
   - Risk: 0.0 (informational)
   - Auto-executable: Yes (just tracking)

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+A` | Toggle autopilot mode |
| `Ctrl+Shift+S` | Show/hide suggestion panel |
| `Ctrl+Shift+Y` | Approve current suggestion |
| `Ctrl+Shift+N` | Reject current suggestion |

## API Reference

### AgenticIDEIntegration

```cpp
// Lifecycle
bool initialize(Win32IDE* ide, HINSTANCE hInstance);
void shutdown();

// Mode control
void toggleAutopilotMode();
void setAutopilotMode(AgenticMode mode);
AgenticMode getAutopilotMode() const;

// Manual triggers
void analyzeCurrentFile(const std::string& filePath);
void analyzeProject(const std::string& projectPath);
void triggerErrorRecovery(const std::string& errorContext);

// Action approval
void approveSuggestion(const std::string& suggestionId);
void rejectSuggestion(const std::string& suggestionId);
void dismissAllSuggestions();

// UI control
void showSuggestionPanel();
void hideSuggestionPanel();
void toggleSuggestionPanel();

// Status
bool hasPendingSuggestions() const;
int getPendingSuggestionCount() const;
std::vector<AgenticSuggestion> getPendingSuggestions() const;
```

## Future Enhancements

1. **Expert Pre-fetcher**: Pre-load MoE experts based on typing patterns
2. **Self-Healing**: Auto-fix compilation errors
3. **Test Generation**: Auto-generate unit tests for functions
4. **Refactoring**: Automated code restructuring
5. **Documentation**: Auto-generate doc comments

## Troubleshooting

### No suggestions appearing
- Check that mode is not Passive
- Verify file is being analyzed (check status bar)
- Look for patterns that trigger suggestions (unused includes, etc.)

### Too many notifications
- Switch to Suggestive mode (queues instead of auto-executes)
- Dismiss suggestions from panel
- Adjust analysis cooldown in `AgenticRouterBridge.cpp`

### Actions failing
- Check AgenticExecutor is properly initialized
- Verify tool implementations in `AgenticExecutor::callTool()`
- Review logs in OutputDebugString

## Files Added

- `src/agentic/AgenticRouterBridge.h` — Decision engine
- `src/agentic/AgenticRouterBridge.cpp` — Implementation
- `src/agentic/AgenticUIBridge.h` — UI layer
- `src/agentic/AgenticUIBridge.cpp` — UI implementation
- `src/agentic/AgenticIDEIntegration.h` — Integration API
- `src/agentic/AgenticIDEIntegration.cpp` — Integration implementation
- `src/agentic/Win32IDE_AgenticIntegration.h` — Drop-in Win32IDE guide
- `src/agentic/AGENTIC_SYSTEM_README.md` — This file

## Integration Status

✅ **Core Components**
- AgenticRouterBridge (decision engine)
- AgenticUIBridge (notification system)
- AgenticIDEIntegration (single integration point)

✅ **Features**
- Three-mode state machine (Passive/Suggestive/Autonomous)
- Background code analysis
- Pattern detection (includes, error handling, memory leaks)
- Suggestion panel with approve/reject
- Notification system
- Status bar integration

⏳ **Pending Integration**
- Wire into Win32IDE::CreateMainMenu()
- Add WM_COMMAND handlers
- Connect to file open events
- Add keyboard shortcuts
- Test with real codebase

## Next Steps

1. **Integrate into Win32IDE.cpp**: Add the member variable and initialization
2. **Add menu items**: Create the Agentic menu in the menu bar
3. **Test modes**: Verify all three modes work correctly
4. **Add patterns**: Extend pattern detection for more issues
5. **Tune thresholds**: Adjust risk/confidence scores based on usage
