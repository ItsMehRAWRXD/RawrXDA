# RawrXD MASM Editor - Quick Start Guide

## Installation & Setup (5 minutes)

### Prerequisites

1. **Windows 7 or later (x64)**
2. **Visual Studio 2019+ with MASM64**
   - Or download MASM64 separately from Microsoft

### Step 1: Download Files

Copy these 3 files to `d:\rawrxd\`:
- `RawrXD_MASM_SyntaxHighlighter.asm`
- `RawrXD_MASM_Editor_Editing.asm`
- `RawrXD_MASM_Editor_MLCompletion.asm`

### Step 2: Build

**Option A - PowerShell (Recommended)**
```powershell
cd d:\rawrxd
.\Build_MASM_Editor.ps1
```

**Option B - Manual**
```cmd
cd d:\rawrxd
ml64.exe RawrXD_MASM_SyntaxHighlighter.asm /c
ml64.exe RawrXD_MASM_Editor_Editing.asm /c
ml64.exe RawrXD_MASM_Editor_MLCompletion.asm /c
link.exe RawrXD_MASM_SyntaxHighlighter.obj ^
         RawrXD_MASM_Editor_Editing.obj ^
         RawrXD_MASM_Editor_MLCompletion.obj ^
         kernel32.lib user32.lib gdi32.lib ^
         /subsystem:windows /entry:main
```

### Step 3: Run

```cmd
RawrXD_MASM_Editor.exe
```

**Expected result:** Dark gray window with title "RawrXD MASM Editor" appears

---

## First Launch

### Window Contents

```
┌──────────────────────────────────────────────────┐
│ RawrXD MASM Editor                          [ _ ] │
├──────┬────────────────────────────────────────────┤
│  Line│ (text editing area - blinking cursor)      │
│   1  │ _                                           │
│   2  │                                             │
│   3  │                                             │
│   4  │                                             │
│   5  │                                             │
└──────┴────────────────────────────────────────────┘
```

### Try Typing

Type your first MASM instruction:

```
mov rax, rbx
```

**Notice:**
- "mov" appears in **orange** (instruction color)
- "rax" appears in **blue** (register color)
- "rbx" appears in **blue** (register color)
- "," appears in **white** (operator color)

---

## Basic Operations

### Typing & Editing

| Action | Keys | Result |
|--------|------|--------|
| Type character | Any key | Insert at cursor |
| Delete left | Backspace | Remove before cursor |
| Delete right | Delete | Remove at cursor |
| New line | Enter | Create line below |
| Undo *(future)* | Ctrl+Z | Revert last change |

### Navigation

| Action | Keys |
|--------|------|
| Move left | ← arrow |
| Move right | → arrow |
| Move up | ↑ arrow |
| Move down | ↓ arrow |
| Line start | Home |
| Line end | End |
| Page up | Page Up |
| Page down | Page Down |

### Text Selection

| Action | Keys |
|--------|------|
| Select left | Shift + ← |
| Select right | Shift + → |
| Select all | Ctrl+A |

---

## Code Completion

### Enable Ollama (Optional But Recommended)

If you want AI-powered code suggestions:

1. **Install Ollama**
   - Download: https://ollama.ai
   - Install and run

2. **Pull Model**
   ```bash
   ollama pull codellama:7b
   ```

3. **Start Ollama Service**
   ```bash
   ollama serve
   # Runs on localhost:11434
   ```

### Using Completions

**Scenario:** You want to complete `add r`

```
1. Type: add r
2. Press: Ctrl+Space
3. See popup with suggestions:
   - add rax, ...
   - add rcx, ...
   - add rsi, ...
4. Use arrow keys to select
5. Press: Enter or Tab
6. Suggestion inserted
```

---

## Syntax Highlighting Examples

### Supported Token Types

```asm
; Comment - GREEN
mov     ; Instruction - ORANGE
rax     ; Register - LIGHT BLUE
.code   ; Directive - MAGENTA
QWORD   ; Type - LIGHT GREEN
"text"  ; String - YELLOW
123     ; Number - LIGHT RED
loop:   ; Label - CYAN
+       ; Operator - WHITE
```

### Example with Colors

```asm
.code
main PROC                           ; DIRECTIVE, INST, Label
    mov rax, 10                    ; INST, REG, NUMBER
    add rax, rbx                   ; INST, REG, REG
    
    ; This is a loop
    mov rcx, 5                     ; INST, REG, NUMBER
    
loop_start:                        ; LABEL
    sub rcx, 1                     ; INST, REG, NUMBER
    jnz loop_start                 ; INST, LABEL
    
    mov r8q, [rsi + 8]            ; INST, REG, INDIRECT
    call some_function             ; INST, LABEL
    
    ret                            ; INST
main ENDP
```

---

## Common Patterns

### Create a Simple Function

```asm
; Type this to practice:

my_function PROC
    mov rax, rcx
    add rax, rdx
    ret
my_function ENDP
```

**Syntax Highlighting:**
- Keywords (PROC, ENDP) → MAGENTA
- Instructions (mov, add, ret) → ORANGE
- Registers (rax, rcx, rdx) → BLUE

### Loop Pattern

```asm
mov rcx, 10          ; Initialize counter

loop_here:           ; Label
    add rax, 1       ; Loop body
    loop loop_here   ; Repeat
```

### Memory Access

```asm
mov rax, [rbx]           ; Load from address in rbx
mov rax, [rbx + 8]       ; Load with offset
mov rax, [rbx + rcx*4]   ; Load with scaled index
mov [rbx], rax           ; Store to address
```

---

## Error Detection

### Syntax Errors

**Type this intentionally:**
```asm
moov rax, rbx    ; Typo: "moov" instead of "mov"
```

**What happens:**
- "moov" is underlined in **RED**
- Status bar shows: "Unknown instruction"

**Fix it:**
1. Backspace to delete extra 'o'
2. Line color changes back to normal
3. "mov" now shows in orange (valid instruction)

### Error Suggestions

**If Ollama is running:**

1. When you type `moov rax, rbx`
2. Press `Ctrl+.` (Ctrl+period)
3. Pop-up shows suggestion: **"Did you mean: mov rax, rbx?"**
4. Press Enter to accept

---

## Tips & Tricks

### Quick Navigation

| Shortcut | Does | Benefit |
|----------|------|---------|
| Ctrl+Home | Jump to start | Instant to top |
| Ctrl+End | Jump to end | Instant to bottom |
| Home | Line start | Avoid arrow spam |
| End | Line end | Avoid arrow spam |

### Editing Efficiency

```
❌ Slow way:
   [Type character]
   [Type character]
   [Type character]
   [Backspace] [Delete] [Backspace]

✅ Fast way:
   [Select word with Shift+arrows]
   [Type replacement]
   [Done - word replaced]
```

### Large Files

For files with 100+ lines:
- Use **Page Up / Page Down** for fast scrolling
- Syntax highlighting still works at 60 FPS
- No performance degradation

---

## Troubleshooting

### Editor Won't Start

**Error: "RawrXD_MASM_Editor.exe not found"**

Solution:
1. Verify build completed: `dir *.exe`
2. If missing, re-run build script
3. Check for compile errors in output

### Syntax Highlighting Not Working

**All text appears white/gray (no colors)**

Possible causes:
- Ollama not running (ML completion needs it)
- Fix: Not required for syntax highlighting to work
- Check: Restart editor after restart to clear cache

**Instructions not highlighted in orange**

- Verify instruction is spelled exactly
- MASM is case-insensitive: `mov`, `MOV`, `Mov` all work
- Check spelling (not "moov", not "mvoi")

### Completion Popup Doesn't Appear

**Cause #1: Ollama not running**
```bash
ollama serve
# In another terminal:
ollama pull codellama:7b
```

**Cause #2: localhost:11434 not accessible**
```cmd
curl http://localhost:11434/api/generate
# Should return: error or valid response
```

**Cause #3: Codellama model not installed**
```bash
ollama list                 ; Check installed models
ollama pull codellama:7b    ; Install if missing
```

---

## Performance Expectations

### Typical Performance

| Operation | Expected Time |
|-----------|----------------|
| Type character | <1ms |
| Render frame (60 FPS) | ~16ms max |
| Search file (10k lines) | <100ms |
| Ctrl+Space popup | 300-500ms |
| Scroll page | <50ms |

### On Slower Machines

- Same functionality, might be ~2x slower
- Still > 20 FPS rendering (visually smooth)

---

## Keyboard Reference Card

### Navigation
```
Arrow keys     Move cursor in 4 directions
Home/End       Start/end of line
Ctrl+Home      Start of document
Ctrl+End       End of document
Page Up/Down   Scroll by screenful
```

### Editing
```
Backspace      Delete character before cursor
Delete         Delete character at cursor
Enter          Insert new line
Ctrl+A         Select all text
Ctrl+X         Cut selection
Ctrl+C         Copy selection
Ctrl+V         Paste
```

### ML Completion
```
Ctrl+Space     Show suggestions
↑ / ↓          Navigate suggestions
Enter/Tab      Accept suggestion
Esc            Dismiss popup
Ctrl+.         Get error fix suggestion
```

---

## Sample Programs

### Print "Hello" Assembly

```asm
.code
main PROC
    sub rsp, 32
    
    ; Call ExitProcess(0)
    xor ecx, ecx            ; ecx = 0
    call ExitProcess        ; System exit
    
main ENDP
```

### Simple Loop

```asm
count_to_10 PROC
    mov rcx, 10             ; Counter = 10
    xor rax, rax            ; Sum = 0
    
loop_start:
    add rax, rcx            ; Sum += counter
    loop loop_start         ; Decrement rcx, loop if not zero
    
    ret
count_to_10 ENDP
```

### Array Access

```asm
sum_array PROC
    ; rcx = array pointer
    ; rdx = count
    xor rax, rax            ; Sum = 0
    xor r8, r8              ; Index = 0
    
loop:
    cmp r8, rdx
    jge done
    
    mov r9d, [rcx + r8*4]   ; Load element (32-bit)
    add rax, r9
    inc r8
    jmp loop
    
done:
    ret
sum_array ENDP
```

---

## Next Steps

### Learning Path

1. ✅ **Done:** Run editor, type text
2. **Next:** Study MASM instruction set
3. **Then:** Write simple functions
4. **After:** Use ML completion for suggestions
5. **Advanced:** Integrate with RawrXD IDE

### Resources

- [x64 Assembly Language Guide](#)
- [Windows x64 Calling Convention](#)
- [MASM Definitive Reference](#)
- [RawrXD Documentation](../README.md)

### Feedback

Found a bug or want a feature?

- Create issue in RawrXD repository
- Tag: `[MASM Editor]`
- Include: steps to reproduce

---

## Keyboard Shortcuts Cheat Sheet

Print this page and keep it handy!

```
NAVIGATION        EDITING           COMPLETION
────────────────  ────────────────  ──────────────
← → ↑ ↓           Backspace         Ctrl+Space
Home              Delete            Ctrl+.
End               Enter             ↑ ↓
Page Up/Down      Select (Shift+↑↓) Enter
                  Cut (Ctrl+X)      Esc
                  Copy (Ctrl+C)
                  Paste (Ctrl+V)
                  All (Ctrl+A)
```

---

## Quick Troubleshooting Flowchart

```
Problem?
│
├─ Editor won't start
│  └─ Run: .\Build_MASM_Editor.ps1 -Rebuild
│
├─ No syntax colors
│  └─ Expected (colors work for MASM keywords only)
│
├─ Ctrl+Space no suggestions
│  └─ Is Ollama running? (ollama serve)
│
├─ Slow rendering
│  └─ Check: File smaller than 100k lines?
│
└─ Application crashed
   └─ Report with error code + last instruction typed
```

---

## Frequently Asked Questions

### Q: Can I edit any file type?
**A:** Yes! Editor works with text files. Syntax highlighting optimized for x64 MASM.

### Q: Do I need Ollama for basic editing?
**A:** No. Syntax highlighting and text editing work without ML. Ollama only enables code completions.

### Q: How many lines can I edit?
**A:** Design supports up to 10,000 lines. Performance optimal up to 5,000 lines.

### Q: Can I change colors/theme?
**A:** Not yet - hardcoded to VS Code dark theme. Customizable in future version.

### Q: Will it work on non-x64 systems?
**A:** No. Editor is compiled as x64-only executable. Requires Windows x64 edition.

### Q: How do I save files?
**A:** Not implemented yet. Planned for next version. Currently use copy-paste to backup.

### Q: Can I import modules from other files?
**A:** Yes - use standard INCLUDE directive:
```asm
INCLUDE MyModule.inc
```

---

## Getting Help

### Resources

1. **This Guide** - Start here (you are reading!)
2. **API Reference** - [RawrXD_MASM_Editor_API_QUICKREF.md](RawrXD_MASM_Editor_API_QUICKREF.md)
3. **Architecture** - [RawrXD_MASM_Editor_ARCHITECTURE.md](RawrXD_MASM_Editor_ARCHITECTURE.md)
4. **Build Guide** - [RawrXD_MASM_Editor_BUILD.md](RawrXD_MASM_Editor_BUILD.md)

### Support Channels

- GitHub Issues: Report bugs
- Discussions: Ask questions
- Pull Requests: Contribute improvements

---

## Congratulations! 🎉

You're now ready to:
- ✅ Write x64 MASM code with syntax highlighting
- ✅ Use AI-powered code completion (if Ollama enabled)
- ✅ Get error suggestions and fixes
- ✅ Navigate and edit efficiently
- ✅ Integrate into RawrXD Autonomy Stack

**Happy coding!**

---

*RawrXD MASM Editor - Quick Start Guide*  
*v1.0 - Get Started in 5 Minutes*

