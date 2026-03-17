# MASM IDE Advanced Features - Quick Reference Card

**Version**: 2.0  
**Date**: December 27, 2025  
**New Features**: Theme Manager, Code Minimap, Command Palette

---

## 🎨 Theme Manager API

### Initialization
```asm
call theme_manager_init              ; Returns: rax=1 (success)
```

### Load Theme
```asm
lea rcx, szThemeDark                 ; "Dark", "Light", "Amber", "Glass", "HighContrast"
call theme_manager_load_theme         ; Returns: rax=1 (success)
```

### Transparency Control
```asm
mov rcx, hMainWindow                 ; Window handle
mov edx, 179                          ; Opacity (0-255, 179=70%)
call theme_manager_set_window_opacity ; Returns: rax=1
```

### Always-on-Top
```asm
mov rcx, hMainWindow
mov edx, 1                            ; 1=enable, 0=disable
call theme_manager_set_always_on_top  ; Returns: rax=1
```

### Get Color
```asm
lea rcx, szColorName                 ; Color slot name
call theme_manager_get_color          ; Returns: rax=0xBBGGRR (packed RGB)
```

### Built-in Themes
| Name | Background | Foreground | Use Case |
|------|-----------|------------|----------|
| Dark | #1E1E1E | #D4D4D4 | Default (VS Code Dark+) |
| Light | #FFFFFF | #000000 | High brightness |
| Amber | #2B1B17 | #FFB000 | Low blue light |
| Glass | #1E1E1E (70% opacity) | #D4D4D4 | Transparency |
| HighContrast | #000000 | #FFFFFF | Accessibility |

---

## 🗺️ Code Minimap API

### Initialization
```asm
call minimap_init                     ; Returns: rax=1
```

### Create Widget
```asm
mov rcx, hMainWindow                 ; Parent window
mov rdx, hEditorControl              ; Text editor handle
call minimap_create_window            ; Returns: rax=hwnd (minimap window)
```

### Update Sync
```asm
call minimap_update                   ; Call on editor text change/scroll
```

### Navigate
```asm
mov ecx, 100                          ; Line number
call minimap_scroll_to_line           ; Scrolls editor to line 100
```

### Configure
```asm
; Set width
mov ecx, 150                          ; Pixels
call minimap_set_width

; Set zoom
movss xmm0, REAL4 ptr [zoom_value]   ; 0.05-0.5 range
call minimap_set_zoom

; Toggle visibility
call minimap_toggle                   ; Show/hide
```

### Features
- **Default Width**: 120px
- **Default Zoom**: 0.1 (10%)
- **Line Height**: 2px per line
- **Double Buffered**: Yes (no flicker)
- **Mouse Support**: Click, drag, wheel zoom

---

## ⌨️ Command Palette API

### Initialization
```asm
call palette_init                     ; Registers 20 default commands
```

### Create Window
```asm
mov rcx, hMainWindow                 ; Parent
call palette_create_window            ; Returns: rax=hwnd (palette popup)
```

### Show/Hide
```asm
call palette_show                     ; Display palette, focus search box
call palette_hide                     ; Hide palette
```

### Register Command
```asm
lea rcx, szCmdID                     ; "custom.action"
lea rdx, szLabel                     ; "Custom: My Action"
lea r8, szShortcut                   ; "Ctrl+Alt+X"
lea r9, callback_function            ; Function pointer
call palette_register_command         ; Returns: rax=1 (success)
```

### Execute Command
```asm
mov ecx, 5                            ; Command index
call palette_execute_command          ; Returns: rax=1 (success)
```

### Default Commands (20 built-in)
| ID | Label | Shortcut |
|----|-------|----------|
| file.new | File: New | Ctrl+N |
| file.open | File: Open | Ctrl+O |
| file.save | File: Save | Ctrl+S |
| edit.undo | Edit: Undo | Ctrl+Z |
| edit.redo | Edit: Redo | Ctrl+Y |
| view.changeTheme | View: Change Theme | - |
| ai.startChat | AI: Start Chat | - |
| model.load | Model: Load GGUF | - |

### Keyboard Shortcuts
- **Ctrl+Shift+P**: Show palette
- **Escape**: Hide palette
- **Enter**: Execute selected
- **Up/Down**: Navigate results

### Limits
- **Max Commands**: 500
- **Max Results**: 15 displayed
- **Max Recent**: 10 tracked

---

## 🏗️ Integration Pattern

### Typical Main Window Initialization
```asm
WinMain PROC
    ; ... Window creation ...
    
    ; Initialize subsystems
    call theme_manager_init
    call minimap_init
    call palette_init
    
    ; Create widgets
    mov rcx, hMainWindow
    mov rdx, hEditorControl
    call minimap_create_window
    mov hMinimap, rax
    
    mov rcx, hMainWindow
    call palette_create_window
    mov hPalette, rax
    
    ; Load default theme
    lea rcx, szThemeDark
    call theme_manager_load_theme
    
    ; Apply transparency
    mov rcx, hMainWindow
    mov edx, 255  ; 100% opaque initially
    call theme_manager_set_window_opacity
    
    ; Message loop...
WinMain ENDP
```

### Window Procedure Hooks
```asm
MainWindowProc PROC
    cmp edx, WM_COMMAND
    je @handle_command
    
    cmp edx, WM_SIZE
    je @handle_size
    
    ; ... other messages ...
    
@handle_command:
    ; Extract menu ID
    mov eax, [rbp + 32]  ; wParam
    and eax, 0FFFFh      ; LOWORD
    
    ; Theme menu
    cmp eax, IDM_VIEW_THEME_DARK
    je @load_dark_theme
    
    ; Minimap toggle
    cmp eax, IDM_VIEW_MINIMAP_TOGGLE
    je @toggle_minimap
    
    ; Command palette
    cmp eax, IDM_VIEW_PALETTE
    je @show_palette
    
    jmp @default_proc
    
@load_dark_theme:
    lea rcx, szThemeDark
    call theme_manager_load_theme
    ; Repaint all windows
    mov rcx, hMainWindow
    call theme_manager_apply_to_window
    ret
    
@toggle_minimap:
    call minimap_toggle
    ret
    
@show_palette:
    call palette_show
    ret
    
@handle_size:
    ; Update minimap on resize
    call minimap_update
    jmp @default_proc
    
@default_proc:
    ; ... DefWindowProcA ...
MainWindowProc ENDP
```

### Editor Event Hooks
```asm
; On text change
EditorWndProc PROC
    cmp edx, WM_CHAR
    je @text_changed
    
    cmp edx, WM_VSCROLL
    je @scroll_changed
    
    jmp @default_proc
    
@text_changed:
    call minimap_update
    jmp @default_proc
    
@scroll_changed:
    call minimap_update
    jmp @default_proc
    
@default_proc:
    call DefWindowProcA
    ret
EditorWndProc ENDP
```

---

## 📊 Memory Layout

### Theme Manager State (g_themeManager)
```
Offset | Size | Field
-------|------|-------
+0     | 132  | currentTheme (THEME_COLORS)
+132   | 64   | currentThemeName
+196   | 1320 | Theme presets (10 × 132 bytes)
+1516  | 1    | transparencyEnabled
+1517  | 1    | alwaysOnTop
+1518  | 1    | clickThroughEnabled
+1520  | 32   | Cached brush handles (4 × 8)
+1552  | 40   | Critical section
Total: ~1,600 bytes
```

### Minimap State (g_minimapState)
```
Offset | Size | Field
-------|------|-------
+0     | 8    | hParentEditor
+8     | 8    | hWindow
+16    | 8    | hBackBuffer
+24    | 8    | hBackBufferDC
+32    | 4    | minimapWidth
+36    | 4    | minimapHeight
+40    | 4    | totalLines
+44    | 4    | visibleLines
+48    | 4    | firstVisibleLine
+52    | 4    | lineHeight
+56    | 4    | zoomFactor (float)
+60    | 1    | isEnabled
+61    | 1    | isDragging
+62    | 1    | showSyntax
+64    | 4    | lastMouseY
+68    | 4    | backgroundColor
+72    | 4    | textColor
+76    | 4    | viewportColor
Total: ~80 bytes
```

### Command Palette State (g_paletteState)
```
Offset | Size | Field
-------|------|-------
+0     | 8    | hWindow
+8     | 8    | hSearchBox
+16    | 8    | hResultsList
+24    | 8    | hFont
+32    | 48000| commands (500 × 96 bytes)
+48032 | 4    | commandCount
+48036 | 120  | results (15 × 8 bytes)
+48156 | 4    | resultCount
+48160 | 4    | selectedIndex
+48164 | 40   | recentCommands (10 × 4)
+48204 | 4    | recentCount
+48208 | 1    | isVisible
+48209 | 1    | isDarkTheme
+48210 | 256  | filterText
Total: ~48,500 bytes
```

**Total Static Memory**: ~50 KB

---

## 🔧 Build Commands

### Assemble
```cmd
ml64 /c /Fo build\obj\masm_theme_manager.obj masm_theme_manager.asm
ml64 /c /Fo build\obj\masm_code_minimap.obj masm_code_minimap.asm
ml64 /c /Fo build\obj\masm_command_palette.obj masm_command_palette.asm
```

### Link
```cmd
link /SUBSYSTEM:WINDOWS /ENTRY:main ^
     build\obj\rawrxd_masm_ide_main.obj ^
     build\obj\masm_theme_manager.obj ^
     build\obj\masm_code_minimap.obj ^
     build\obj\masm_command_palette.obj ^
     kernel32.lib user32.lib gdi32.lib comctl32.lib ^
     /OUT:build\bin\RawrXD_MASM_IDE.exe
```

### Full Build
```cmd
cd src\masm\final-ide
build_masm_ide.bat
```

---

## 🐛 Troubleshooting

### Theme Manager Issues

**Problem**: Theme doesn't apply  
**Solution**: Call `theme_manager_apply_to_window()` after load

**Problem**: Transparency not working  
**Solution**: Check WS_EX_LAYERED style is set (handled automatically)

**Problem**: Always-on-top fails  
**Solution**: Verify SetWindowPos succeeds (check return value)

### Minimap Issues

**Problem**: Minimap not updating  
**Solution**: Hook editor EN_CHANGE notification → `minimap_update()`

**Problem**: Click navigation incorrect  
**Solution**: Verify editor uses EM_LINESCROLL message

**Problem**: Flicker during scroll  
**Solution**: Double buffering enabled by default (check back buffer creation)

### Command Palette Issues

**Problem**: Commands not showing  
**Solution**: Check `palette_register_command()` return value

**Problem**: Fuzzy search not working  
**Solution**: Currently stub - shows all commands (implement later)

**Problem**: Callbacks not firing  
**Solution**: Verify function pointer is valid (not NULL)

---

## 📚 References

- **Theme Manager**: `src/masm/final-ide/masm_theme_manager.asm`
- **Code Minimap**: `src/masm/final-ide/masm_code_minimap.asm`
- **Command Palette**: `src/masm/final-ide/masm_command_palette.asm`
- **Build Script**: `src/masm/final-ide/build_masm_ide.bat`
- **Full Docs**: `src/masm/final-ide/QT_PARITY_IMPLEMENTATION_COMPLETE.md`

---

## ✅ Checklist for Integration

- [ ] Call `theme_manager_init()` in WinMain
- [ ] Call `minimap_init()` in WinMain
- [ ] Call `palette_init()` in WinMain
- [ ] Create minimap after editor creation
- [ ] Create palette popup window
- [ ] Register Ctrl+Shift+P accelerator
- [ ] Add theme menu items
- [ ] Hook editor text change to minimap
- [ ] Wire command callbacks
- [ ] Test theme switching
- [ ] Test minimap navigation
- [ ] Test command execution

---

**Quick Start**: Copy-paste the integration pattern above into your main window file!
