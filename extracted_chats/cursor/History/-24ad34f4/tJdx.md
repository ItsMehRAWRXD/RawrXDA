# Menu and Chat Audit (kli worktree)

## Summary of fixes applied

### Ship IDE (`Ship/Win32_IDE_Complete.cpp`)
- **Chat panel:** Send button is created in `CreateChatPanel()` with ID `IDC_CHAT_SEND` (2008), shown/hidden with View > Chat and AI > Chat.
- **Layout:** Chat strip positions input (width = chatW - 56 - 4) and Send button (56×24) at bottom-right; `LayoutControls()` moves both.
- **Enter to send:** Chat input is subclassed with `ChatInputSubclassProc`; Enter sends via `SendChatMessage()` and clears the edit.
- **Send button:** `WM_COMMAND` case `IDC_CHAT_SEND` calls `SendChatMessage()` with the input text.

### Main IDE (`src/win32app/`)
- **Tools > Settings:** Menu item added with ID **502** in `createMenuBar()` (Tools menu). `onCommand` in `Win32IDE_Core.cpp` already had `case 502:` (and 1024, 1106) calling `showSettingsGUIDialog()`.
- **Command Palette:** Ctrl+Shift+P was already handled in the message loop in `Win32IDE_Core.cpp` for any control. A **WM_KEYDOWN** handler was added in the main window’s `WindowProc` so the palette also opens when the main window (frame) has focus.

## Menu command dispatch (main IDE)

1. **WM_COMMAND** → `onCommand(hwnd, id, ...)` in `Win32IDE_Core.cpp`.
2. **Explicit cases** in `onCommand`: 502 (Settings), 1024, 1106, 1022, 3007, 3009, 2026, 2027, etc.
3. **routeCommand(id):** View/Tier1/Git and other legacy paths (`handleViewCommand`, `handleTier1Command`, etc.) in `Win32IDE_Commands.cpp` and related files.
4. **routeCommandUnified(id):** All commands in `COMMAND_TABLE` (command_registry.hpp); same path as CLI.
5. **Unknown id:** Status bar message “Unknown command (id %d)” and `DefWindowProc`.

So every menu ID is either handled by an explicit case, by `routeCommand`, or by `routeCommandUnified`. No menu item is left without a path; unknown IDs are reported on the status bar.

## Key files

| Area | File |
|------|------|
| Ship chat panel, Send, layout | `Ship/Win32_IDE_Complete.cpp` |
| Main IDE menu bar | `src/win32app/Win32IDE.cpp` (`createMenuBar`) |
| Main IDE command handling | `src/win32app/Win32IDE_Core.cpp` (`onCommand`, message loop, WM_KEYDOWN) |
| Command palette UI | `src/win32app/Win32IDE_Commands.cpp` |
| View/Tier1 routing | `src/win32app/Win32IDE_Commands.cpp` (`routeCommand`, `handleViewCommand`) |

## Verification checklist

- [x] Ship: Chat panel shows; Send button visible and laid out next to input.
- [x] Ship: Enter in chat input sends message; Send button sends message.
- [x] Main IDE: Tools > Settings opens Settings dialog (ID 502).
- [x] Main IDE: Ctrl+Shift+P opens Command Palette (message loop + main window WM_KEYDOWN).
- [x] Main IDE: Menu IDs either handled in onCommand/routeCommand/routeCommandUnified or reported as unknown.
