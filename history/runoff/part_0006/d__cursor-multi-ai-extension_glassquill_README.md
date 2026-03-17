# GlassQuill: Transparent OpenGL Overlay Text Pad (D:\ only)

## Summary
- Borderless layered Win32 window with OpenGL content
- Real-time opacity slider (global alpha via SetLayeredWindowAttributes)
- Minimal pixel-font editor (6x8 glyphs), caret, type/backspace/enter
- No external libraries and no writes outside D:\

## Build
- Open "x64 Native Tools Command Prompt for VS"
- cd D:\cursor-multi-ai-extension\glassquill
- build.bat
- .\GlassQuill.exe

## Controls
- Drag the round slider knob to adjust window opacity
- Click in the text region to focus, then type ASCII text
- Backspace deletes, Enter inserts newline
- F2 toggles always-on-top
- ESC quits

## Notes
- Uses raw Win32 + WGL (OpenGL 1.1) and a procedurally generated 6x8 atlas
- Designed as a base for future: GIF backgrounds, per-layer opacity, shader effects, voice I/O
