# RawrXDLoops - Pure MASM64 Music Production DAW

A complete digital audio workstation built entirely in pure MASM64 assembly language with zero external dependencies.

## Features

- Pure MASM64 implementation (no C/C++ runtime)
- Real-time audio processing with WASAPI
- 5-pane interface (Channel Rack, Piano Roll, Playlist, Mixer, Browser)
- Agentic AI integration for beat generation
- Hot-reload capability for live code updates
- Sample-accurate audio engine
- Professional-grade DSP algorithms

## Build Instructions

```batch
ml64 /c RawrXDLoops.asm
link /subsystem:windows /entry:WinMain RawrXDLoops.obj kernel32.lib user32.lib gdi32.lib dsound.lib winmm.lib
```

## Project Structure

- `RawrXDLoops.asm` - Main DAW application
- `build.bat` - Build script
- `samples/` - Sample library
- `presets/` - Plugin presets
- `docs/` - Documentation

## Usage

1. Run `build.bat` to compile
2. Launch `RawrXDLoops.exe`
3. Use the chat pane to generate beats: "techno", "acid", "ambient"
4. Hot-reload new features by typing commands

## Agentic Commands

- "techno" - Sets BPM to 135, cleaner mix
- "acid" - Sets resonance to 0.95, squelch maximized
- "ambient" - Sets BPM to 70, low-pass filter
- "add reverb" - Adds reverb module
- "change bpm 130" - Changes tempo to 130 BPM