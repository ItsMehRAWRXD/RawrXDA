# Zero-Day Settings Quick Reference

## Global Variables
Access these from zero_day_integration.asm to make routing decisions:

```asm
; In zero_day_settings_handler.asm
EXTERN gZeroDayForceMode:DWORD          ; 0=normal, 1=force
EXTERN gComplexityThreshold:DWORD       ; 0-100 range

; In RouteExecution or similar:
mov eax, [gZeroDayForceMode]
test eax, eax
jnz use_zero_day_engine      ; If forced, always use zero-day
; Otherwise continue with complexity-based routing
```

## Menu Item IDs (Tools Menu)
- `IDM_TOOLS_ZERO_DAY` = 4005 → "Zero-Day Settings"
- `IDM_TOOLS_ZERO_DAY_FORCE` = 4006 → "Force Complex Goals (Zero-Day)"

## Function Signatures

### zero_day_settings_handler.asm

```asm
; Initialize settings from persistent storage
EXTERN zero_day_settings_init:PROC
; Signature: void zero_day_settings_init()

; Toggle force mode and persist setting
EXTERN zero_day_settings_toggle_force_mode:PROC
; Signature: DWORD zero_day_settings_toggle_force_mode()
; Returns: New mode value (0 or 1)

; Set complexity threshold (0-100)
EXTERN zero_day_settings_set_threshold:PROC
; Signature: void zero_day_settings_set_threshold(DWORD threshold)
; Auto-clamps to 0-100 range

; Get current force mode
EXTERN zero_day_settings_get_force_mode:PROC
; Signature: DWORD zero_day_settings_get_force_mode()
; Returns: Current mode value (0 or 1)

; Get current complexity threshold
EXTERN zero_day_settings_get_threshold:PROC
; Signature: DWORD zero_day_settings_get_threshold()
; Returns: Current threshold (0-100)
```

### menu_dispatch.asm

```asm
; Route menu command ID to handler
EXTERN menu_dispatch_command:PROC
; Signature: DWORD menu_dispatch_command(DWORD commandId)
; Call with: mov ecx, IDM_TOOLS_ZERO_DAY_FORCE; call menu_dispatch_command
```

## Integration Example (zero_day_integration.asm)

```asm
; At top of RouteExecution or decision point:
EXTERN gZeroDayForceMode:DWORD

; In your routing logic:
mov eax, [gZeroDayForceMode]
test eax, eax
jnz route_to_zero_day

; Normal path - check complexity
cmp [complexity_level], 50
jge route_to_zero_day
; Route to simple/fallback engine

route_to_zero_day:
; Use zero-day agentic engine
```

## Menu Handler Flow

1. **User Action**: Click "Force Complex Goals (Zero-Day)" in Tools menu
2. **Windows**: Sends WM_COMMAND with wParam = 4006
3. **Dispatch**: menu_dispatch_command(4006) called
4. **Handler**: menu_tools_zero_day_force() executes
5. **Toggle**: zero_day_settings_toggle_force_mode() called
6. **Persist**: Settings saved via masm_settings_set_bool()
7. **Notify**: Status message shown to user
8. **Effect**: Immediate - gZeroDayForceMode updated, next execution uses new setting

## Settings Persistence

Settings automatically saved to registry (Windows) or config file:
- Key: "zero_day_force_complex_goals" (BOOL)
- Key: "complexity_threshold" (INT)
- Location: Software\RawrXD\AgenticIDE (registry) or config.ini

Initialize on startup:
```asm
call zero_day_settings_init  ; Load from persistent storage
```

## Status Messages

When user toggles "Force Complex Goals":

**ENABLED Message:**
```
Force Complex Goals (Zero-Day) ENABLED
The agentic pipeline will always use zero-day engine 
regardless of complexity detection.
```

**DISABLED Message:**
```
Force Complex Goals (Zero-Day) DISABLED
Complexity will be detected automatically 
(simple->zero-day, complex->enterprise).
```

## Complexity Scale (0-100)
- 0-25: Simple goals → Fallback engine
- 26-50: Moderate goals → Fallback/Zero-day hybrid
- 51-75: Complex goals → Zero-day engine
- 76-100: Expert goals → Zero-day with advanced features

When **Force Mode = 1**, complexity checks are bypassed and zero-day is always used.

## Testing Commands

From MASM code, trigger menu handlers directly:
```asm
; Manually toggle force mode
call zero_day_settings_toggle_force_mode

; Check current state
call zero_day_settings_get_force_mode
test eax, eax
jz mode_is_off
; mode_is_on

; Adjust threshold
mov ecx, 75  ; Set to 75%
call zero_day_settings_set_threshold
```

## Debug Output

Menu operations logged to output pane:
- "[Zero-Day] Settings panel opened"
- "[Zero-Day] Force complex goals mode toggled"

Add logging to your routing decision:
```asm
mov eax, [gZeroDayForceMode]
test eax, eax
jz .normal_route
; Log: "Using zero-day engine (forced)"
jmp .use_zero_day
.normal_route:
; Continue with complexity-based routing
```

## Next Steps

1. **Wire into zero_day_integration.asm**:
   - Add extern declarations for gZeroDayForceMode
   - Add check in RouteExecution() before complexity switch
   
2. **Create settings dialog** (future):
   - Slider for complexity threshold
   - Checkbox for force mode
   - Real-time statistics
   
3. **Add keyboard shortcut** (future):
   - Ctrl+Alt+Z for quick toggle
   
4. **Extend logging** (future):
   - Track all mode changes with timestamps
   - Collect metrics on engine selection frequency
