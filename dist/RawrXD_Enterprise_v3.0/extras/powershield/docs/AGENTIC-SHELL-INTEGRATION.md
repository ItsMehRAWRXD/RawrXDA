╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║     🚀 RAWRXD AGENTIC SHELL INTEGRATION - COMPLETE 🚀                       ║
║                                                                              ║
║              Keyboard Shortcuts & Commands for Agentic IDE                   ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝


🎯 KEYBOARD SHORTCUTS
═══════════════════════════════════════════════════════════════════════════════

Available in RawrXD editor:

  Ctrl+Shift+G    →  Generate Code
                     Prompt: "What code do you want to generate?"
                     Result: Copied to clipboard

  Ctrl+Shift+A    →  Analyze Code
                     Source: Gets code from clipboard
                     Result: Shows analysis in dev console

  Ctrl+Shift+R    →  Refactor Code
                     Source: Gets code from clipboard
                     Result: Copied to clipboard

  Ctrl+Shift+S    →  Agentic Status
                     Shows: Current model, temperature, functions
                     Result: Displayed in console


📊 MENU COMMANDS
═══════════════════════════════════════════════════════════════════════════════

Available from Tools/Agentic menu (when registered):

  🚀 Generate Code
     Keyboard: Ctrl+Shift+G
     Prompts for code description
     Generates and copies to clipboard

  🔍 Analyze Code
     Keyboard: Ctrl+Shift+A
     Takes clipboard content
     Shows improvement suggestions

  ♻️ Refactor Code
     Keyboard: Ctrl+Shift+R
     Takes clipboard content
     Modernizes and optimizes

  📊 Agentic Status
     Shows current agentic mode status
     Model information
     Available functions


💻 SHELL COMMANDS (PowerShell)
═══════════════════════════════════════════════════════════════════════════════

Use in the integrated terminal or PowerShell:

# Generate code
Invoke-AgenticShellCommand -Command 'generate' -Parameters @{
    prompt = "Create a function to parse JSON"
    language = "powershell"
}

# Analyze code
Invoke-AgenticShellCommand -Command 'analyze' -Parameters @{
    code = @'
function Get-Data {
    $result = [int]"abc"
    return $result
}
'@
    type = "improve"
}

# Refactor code
Invoke-AgenticShellCommand -Command 'refactor' -Parameters @{
    code = $oldCode
    objective = "modernize"
}

# Get completions
Invoke-AgenticShellCommand -Command 'complete' -Parameters @{
    partial = "function Get-"
    context = "cmdlet"
}

# Check status
Invoke-AgenticShellCommand -Command 'status'


🔄 WORKFLOW EXAMPLES
═══════════════════════════════════════════════════════════════════════════════

EXAMPLE 1: Generate → Analyze → Copy
─────────────────────────────────────────────────────────────────────────────
1. Press Ctrl+Shift+G
2. Type: "Create a function to list all processes"
3. Code generated and copied
4. Paste into editor
5. Press Ctrl+Shift+A to analyze
6. Review suggestions

EXAMPLE 2: Paste → Refactor → Compare
─────────────────────────────────────────────────────────────────────────────
1. Copy old code to clipboard
2. Press Ctrl+Shift+R
3. Refactored code copied
4. Paste into new file
5. Compare with original

EXAMPLE 3: Debug via Analysis
─────────────────────────────────────────────────────────────────────────────
1. Copy buggy code to clipboard
2. Press Ctrl+Shift+A
3. Select "debug" from analysis types
4. See bugs and fixes
5. Apply suggestions

EXAMPLE 4: Batch Processing
─────────────────────────────────────────────────────────────────────────────
# In PowerShell terminal:
$files = Get-ChildItem *.ps1
foreach ($file in $files) {
    $code = Get-Content $file.FullName -Raw
    $improved = Invoke-AgenticShellCommand -Command 'analyze' `
        -Parameters @{code=$code; type='improve'}
    $improved | Out-File "improved-$($file.Name)"
}


🎮 EDITOR INTEGRATION POINTS
═══════════════════════════════════════════════════════════════════════════════

The shell integration provides:

✓ Hotkey Detection
  - Registers Ctrl+Shift+G/A/R in editor form
  - Handles key events efficiently
  - Non-blocking operations

✓ Clipboard Integration
  - Read from: Paste code to analyze/refactor
  - Write to: Copy generated/refactored code
  - Format: Plain text PowerShell compatible

✓ Menu Items
  - New "Agentic" menu in Tools section
  - Sub-items for each command
  - Visual indicators (🚀 🔍 ♻️ 📊)

✓ Developer Console
  - Real-time output of commands
  - Color-coded messages
  - Error tracking and display

✓ Command Routing
  - Route through Process-AgentCommand
  - Validated and safe execution
  - Full logging and error handling


🔐 SECURITY & RELIABILITY
═══════════════════════════════════════════════════════════════════════════════

✓ Input Validation
  - All commands validated before execution
  - Safety checks for code injection
  - Parameter validation

✓ Error Handling
  - Try-catch around all operations
  - Graceful fallback on errors
  - User-friendly error messages

✓ Logging
  - All commands logged to startup log
  - Success/warning/error tracking
  - Debug information available

✓ Performance
  - Non-blocking shell commands
  - Async clipboard operations
  - Minimal overhead


📈 ADVANCED USAGE
═══════════════════════════════════════════════════════════════════════════════

# Custom Language for Generation
Invoke-RawrXDAgenticCodeGen `
    -Prompt "Create REST API handler" `
    -Language "csharp"

# Batch Code Analysis
Get-ChildItem *.ps1 | ForEach-Object {
    $code = Get-Content $_
    Invoke-AgenticShellCommand -Command 'analyze' `
        -Parameters @{code=$code; type='debug'}
}

# Chained Operations
$code = Invoke-RawrXDAgenticCodeGen -Prompt "Fibonacci function"
$improved = Invoke-AgenticShellCommand -Command 'analyze' `
    -Parameters @{code=$code; type='improve'}
$final = Invoke-AgenticShellCommand -Command 'refactor' `
    -Parameters @{code=$improved; objective='optimize'}

# Analysis Types
'improve'   → Code quality and best practices
'debug'     → Bug detection and fixes
'refactor'  → Modernization and simplification
'test'      → Unit test generation
'document'  → Auto-documentation


🎨 CUSTOMIZATION
═══════════════════════════════════════════════════════════════════════════════

Modify hotkeys in Register-AgenticHotkeys function:

# Change Ctrl+Shift+G to Ctrl+Alt+G:
$FormControl.Add_KeyDown({
    if ($_.Control -and $_.Alt -and $_.KeyCode -eq [System.Windows.Forms.Keys]::G) {
        ...
    }
})

Add custom commands to Invoke-AgenticShellCommand:

function Invoke-AgenticShellCommand {
    switch ($Command) {
        'mycommand' {
            # Your custom logic here
        }
    }
}


📚 INTEGRATION FEATURES
═══════════════════════════════════════════════════════════════════════════════

Functions Added to RawrXD.ps1:

• Invoke-AgenticShellCommand
  - Route agentic commands from UI/hotkeys
  - Parameter validation
  - Result handling

• New-AgenticEditorMenu
  - Create agentic items in menu bar
  - Wire up click handlers
  - Add to Tools menu automatically

• Register-AgenticHotkeys
  - Register Ctrl+Shift+G/A/R
  - Non-blocking key event handlers
  - Integration with editor form

Command Routing:

• Existing Process-AgentCommand extended
  - Added agentic.generate
  - Added agentic.analyze
  - Added agentic.refactor
  - Added agentic.complete
  - Added agentic.status


🚀 QUICK START
═══════════════════════════════════════════════════════════════════════════════

1. Launch RawrXD:
   .\RawrXD.ps1

2. Wait for IDE to load

3. Try a hotkey:
   Ctrl+Shift+G    (generate code)

4. Or use the menu:
   Tools → Agentic → Generate Code

5. Or run in terminal:
   Invoke-AgenticShellCommand -Command 'generate'


🎯 WHAT'S NOW AVAILABLE
═══════════════════════════════════════════════════════════════════════════════

Inside RawrXD.ps1, you now have:

✅ Shell Command Routing      - Process agentic commands
✅ Hotkey Integration          - Ctrl+Shift+G/A/R shortcuts
✅ Menu Items                  - Agentic operations menu
✅ Clipboard Integration       - Copy/paste code easily
✅ Developer Console Output    - Real-time feedback
✅ Error Handling              - Graceful failure modes
✅ Command Logging             - Full audit trail
✅ Extensible Architecture     - Easy to add more commands


═══════════════════════════════════════════════════════════════════════════════

Your RawrXD IDE now has full shell integration for agentic operations!

Use the keyboard shortcuts or menu to generate, analyze, refactor, and
optimize your code directly from the editor.

Start: Ctrl+Shift+G to generate your first agentic code! 🚀

═══════════════════════════════════════════════════════════════════════════════
