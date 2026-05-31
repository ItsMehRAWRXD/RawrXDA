# 🚀 Sovereign Integration - Phase 1: Console Output Routing

## Immediate Implementation Steps

### Step 1: Add Sovereign Console Type
**File:** `src/win32app/Win32TerminalManager.h`

```cpp
enum ShellType {
    PowerShell,
    CommandPrompt,
    SovereignConsole  // ADD THIS LINE
};
```

### Step 2: Create Sovereign Console Startup
**File:** `src/win32app/Win32TerminalManager.cpp`

```cpp
bool Win32TerminalManager::startSovereignConsole(const char* modelPath) {
    // Stop any existing shell
    stop();
    
    // Build Sovereign command
    std::string command = "sovereign_finisher.exe";
    if (modelPath && modelPath[0] != '\0') {
        command += " ";
        command += modelPath;
    }
    
    // Start with pipes (not ConPTY for now)
    return startWithPipes(command, nullptr);
}
```

### Step 3: Add Sovereign Terminal Pane Creation
**File:** `src/win32app/Win32IDE.cpp`

```cpp
// Add to createTerminalPane function
case Win32TerminalManager::SovereignConsole:
    pane.manager->startSovereignConsole();
    break;
```

### Step 4: Add Sovereign-specific Output Handling
**File:** `src/win32app/Win32IDE.cpp`

```cpp
// Modify terminal output handler to detect Sovereign
void Win32IDE::onTerminalOutput(int paneId, const std::string& output) {
    auto pane = findTerminalPane(paneId);
    if (!pane) return;
    
    // Check if this is a Sovereign console
    bool isSovereign = (pane->shellType == Win32TerminalManager::SovereignConsole);
    
    std::string formattedOutput = output;
    if (isSovereign) {
        // Add Sovereign prefix and special formatting
        formattedOutput = "[Sovereign] " + output;
    }
    
    // Existing output handling code
    appendText(pane->hwnd, formattedOutput);
}
```

### Step 5: Add Sovereign Console Menu Option
**File:** `src/win32app/Win32IDE.cpp`

```cpp
// Add to menu handler
case ID_TERMINAL_SOVEREIGN_CONSOLE: {
    createTerminalPane(Win32TerminalManager::SovereignConsole, 
                      "Sovereign Console", 
                      TerminalPaneKind::UserInteractive, 
                      true);
    break;
}
```

## Testing This Phase

### Test 1: Basic Console Startup
```cpp
// Simple test to verify Sovereign console starts
Win32TerminalManager manager;
bool success = manager.startSovereignConsole();
assert(success == true);
```

### Test 2: Output Routing
```cpp
// Test output appears in GUI terminal
manager.writeInput("open test.txt\n");
// Should see "[Sovereign] [OK] Opened: test.txt" in terminal pane
```

### Test 3: Menu Integration
```cpp
// Test menu option creates Sovereign terminal
// Click Terminal → New Sovereign Console
// Verify new terminal pane with Sovereign prefix
```

## Expected Results After Phase 1

✅ Sovereign console opens in GUI terminal pane
✅ Console output appears with "[Sovereign]" prefix
✅ Basic commands work (open, save, insert, etc.)
✅ No regression in existing terminal functionality

## Next Steps After Phase 1

1. **Phase 2**: Inference engine switching
2. **Phase 3**: Autonomous mode integration
3. **Phase 4**: Advanced feature connectivity

This first phase provides immediate visible integration and sets the foundation for the complete Sovereign IDE integration into the main GUI IDE.