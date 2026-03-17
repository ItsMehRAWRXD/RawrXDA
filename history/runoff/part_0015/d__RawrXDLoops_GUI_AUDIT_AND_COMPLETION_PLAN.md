# Underground King DAW: Complete GUI Audit & Implementation Plan

## EXECUTIVE SUMMARY

**Current Status**: omega.asm (2743 lines) is a **functional but incomplete** MASM64 DAW with:
- ✅ Working window system and message loop
- ✅ 5-panel layout (Channel Rack, Piano Roll, Playlist, Mixer, Browser)
- ✅ 16-step sequencer grid with mouse interaction
- ✅ 5 genre pattern generators (Techno, Acid, Ambient, Hip-Hop, House)
- ✅ Real-time audio synthesis (kick, snare, hihat, bass)
- ✅ Professional DSP chain (SVF filter, distortion, delay, reverb, compression)
- ✅ Oscilloscope visualization
- ✅ Build system producing working executable

**Missing**: Professional GUI controls and workflows (transport UI, faders, EQ curves, automation, file browser, menus)

**Completion Estimate**: 8-12 additional components to build a professional DAW interface

---

## PART 1: DETAILED AUDIT OF CURRENT IMPLEMENTATION

### 1.1 ARCHITECTURE OVERVIEW

**File**: `omega.asm` (2743 lines total)
**Entry Point**: `WinMain` (lines ~420)
**Main Message Handler**: `WndProc` (lines ~550-620)
**Drawing System**: `DrawEnhancedUI` (lines ~670-680)

```
WinMain
  └─ RegisterMainWindow
  └─ CreateMainWindow
  └─ InitializeAudio
  └─ LoadGenrePattern
  └─ SetTimer(TIMER_UI, 50ms)
  └─ SetTimer(TIMER_SEQUENCER, dwMsPerStep)
  └─ Message Loop
       ├─ WM_PAINT → DrawEnhancedUI
       ├─ WM_TIMER → TriggerAudioBuffer (if playing)
       ├─ WM_KEYDOWN → Keyboard handlers
       └─ WM_LBUTTONDOWN → HandleGridClick
```

---

### 1.2 IMPLEMENTED GUI COMPONENTS

#### **1.2.1 Window System** ✅ COMPLETE
- **Function**: `RegisterMainWindow`, `CreateMainWindow`
- **Resolution**: 1600×700 pixels (fixed)
- **Window Flags**: WS_OVERLAPPEDWINDOW | WS_VISIBLE
- **Update Rate**: 50ms timer (20 FPS UI)
- **Status**: Production-ready

#### **1.2.2 5-Panel Layout** ✅ COMPLETE
- **Function**: `DrawPanelLayout` (lines 690-770)
- **Regions**:
  ```
  Panel 1 (Channel Rack):  x=0-300,    y=50-650    [background: #202020]
  Panel 2 (Piano Roll):    x=302-700,  y=50-650    [background: #202020]
  Panel 3 (Playlist):      x=702-1100, y=50-650    [background: #202020]
  Panel 4 (Mixer):         x=1102-1400, y=50-650   [background: #202020]
  Panel 5 (Browser):       x=1402-1600, y=50-650   [background: #202020]
  ```
- **Active Panel Indicator**: Orange border (x += 2, width = 296) at top (y=50-650)
- **Text Labels**: Rendered at (x+10, y=55) for each panel
- **Status Bar**: y=0-45 (dark background #181818)
- **Color Palette**:
  - Background: #202020 (dark gray)
  - Active Indicator: #FF6B35 (orange)
  - Text: #E0E0E0 (light gray)
  - Status Bar: #181818 (very dark)
- **Status**: Fully functional and responsive

#### **1.2.3 16-Step Sequencer Grid** ✅ COMPLETE
- **Function**: `DrawChannelRack` (lines 776-870), `HandleGridClick` (lines 1045-1095)
- **Layout**: 
  - Rows: 8 channels (28px height each)
  - Columns: 16 steps (17px width each)
  - Grid origin: x=10, y=80
  - Grid size: 272 × 280 pixels
- **Cell States**: 
  - Off: dark gray background
  - On: light orange (#FF6B35)
  - Border: light gray outline
- **Mouse Interaction**:
  - Click toggles cell state (stored in `g_PatternData` byte array)
  - Coordinate mapping: Converts LBUTTONDOWN position to (step, channel)
  - Division logic: `step = (x-10) / 17`, `channel = (y-80) / 35`
- **Data Structure**: `g_PatternData[MAX_CHANNELS * MAX_STEPS]` = 128 bytes (8×16)
- **Pattern Loading**: 5 hardcoded genre patterns (Techno, Acid, Ambient, Hip-Hop, House)
- **Status**: Fully functional with real-time toggle feedback

#### **1.2.4 Real-Time Oscilloscope Visualization** ✅ COMPLETE
- **Function**: `DrawOscilloscope` (lines 976-1040)
- **Display Area**: x=302, y=80 (within Piano Roll panel)
- **Size**: 396 pixels wide × 200 pixels tall
- **Waveform Rendering**:
  - Reads from `g_AudioBuffer1` (most recently generated)
  - Plots 512 samples at 44.1kHz → ~11.6ms window
  - X scaling: 396 pixels / 512 samples ≈ 0.77 px/sample
  - Y scaling: Center at y=180, amplitude ±100 pixels
- **Color**: Light gray (#E0E0E0) lines with #202020 background
- **Performance**: Drawn every UI frame (50ms refresh)
- **Status**: Functional real-time visualization

#### **1.2.5 Audio Synthesis & DSP Chain** ✅ COMPLETE
- **Function**: `GenerateAudioBuffer` (lines 1300-1500)
- **Sample Rate**: 44.1 kHz, 16-bit stereo
- **Buffer Size**: 4410 samples (~100ms of audio)
- **Double Buffering**: `g_AudioBuffer1` and `g_AudioBuffer2` with ping-pong switching
- **Synthesis Engines**:
  ```
  Kick:   SSO (Simple Sine Oscillator) @ g_fKickPitch (55 Hz)
          Envelope: g_fKickEnvelope with decay 0.9999/sample
          Trigger: Pattern data + phase reset
  
  Snare:  Mixed sine + filtered noise
          Frequencies: 200Hz (primary) + 127Hz (secondary)
          Envelope: g_fSnareEnvelope with decay 0.995/sample
          Trigger: Pattern data + phase reset
  
  Hihat:  Noise generator (RDTSC XOR for pseudo-random)
          Frequency filtering via amplitude envelope
          Envelope: g_fHihatEnvelope with decay 0.99/sample
  
  Bass:   SSO @ 50 Hz
          Envelope: g_fBassEnvelope with decay 0.9999/sample
          Panning: g_fMixerPan0 (-1.0 = left, 0.0 = center, 1.0 = right)
  ```
- **Master Processing**:
  - Master volume: `g_fMasterVolume` (default 0.75)
  - Soft clipping: min/max to [-1.0, 1.0]
  - 16-bit conversion: multiply by 32767, cvtss2si to int16
  - Stereo output: Left + Right channels written as 4-byte pairs
- **DSP Effects Chain** (called after buffer generation):
  ```
  1. ApplySVFilter          - State Variable Filter (12dB/oct)
  2. ApplyDistortion        - Soft clipping + drive
  3. ApplyDelay             - Echo (up to 1 second)
  4. ApplyReverb            - Schroeder reverberator (4 parallel delay lines)
  5. ApplyCompressor        - Dynamic range processor with envelope follower
  ```
- **Status**: Production-quality synthesis with multiple oscillator types (see below)

#### **1.2.6 DSP Effect Implementations** ✅ COMPLETE

**State Variable Filter (SVF)** (lines 1620-1750):
- Type: 12dB/octave multimode (low-pass, band-pass, high-pass, notch)
- Coefficient: `F = 2*sin(π*cutoff/SR)` approximated
- Parameters:
  - `g_fSVF_Cutoff` (default 8000 Hz)
  - `g_fSVF_Resonance` (Q factor, 0.0-1.0)
- Outputs: Low, Band, High, Notch stored separately
- Status: Functional, could use resonance calibration

**Distortion** (lines 1760-1850):
- Types: Soft clipping + saturation curve
- Parameters:
  - `g_fDistDrive` (0.0-1.0, controls input gain)
  - `g_fDistMix` (wet/dry blend)
  - `g_fDistGain` (makeup gain, default 1.5)
- Curve: arctan approximation for smooth saturation
- Status: Working, could add more saturation models

**Delay** (lines 1860-1950):
- Type: Feedback delay (echo)
- Buffer: `g_DelayBuffer` (44100 samples = 1 second @ 44.1kHz)
- Parameters:
  - `g_fDelayTime` (0.0-1.0 maps to 0-1 second)
  - `g_fDelayFeedback` (0.4 default)
  - `g_fDelayMix` (wet/dry, 0.3 default)
- Operation: Write at position, read from delayed position, feedback
- Status: Working echo effect

**Reverb** (Schroeder-style, lines 1960-2100):
- Type: Parallel delay lines with feedback + damping
- Buffers: 4 parallel lines (1557, 1617, 1491, 1422 sample lengths = primes)
- Parameters:
  - `g_fReverbMix` (0.35 default)
  - `g_fReverbDecay` (0.85 default)
  - `g_fReverbDamp` (0.5 default, high-freq damping)
- Operation: Each line independently processing, outputs summed
- Status: Professional-quality ambient reverb

**Compressor** (Envelope follower, lines 2110-2200):
- Type: Dynamic range processor with attack/release
- Parameters:
  - `g_fCompThreshold` (0.7 default)
  - `g_fCompRatio` (4.0 default)
  - `g_fCompAttack` (0.001 seconds)
  - `g_fCompRelease` (0.1 seconds)
  - `g_fCompEnvelope` (state variable)
  - `g_fCompGain` (makeup gain, 1.0 default)
- Behavior: Detects input level, applies gain reduction above threshold
- Status: Working dynamics processor

#### **1.2.7 Keyboard Input Handling** ✅ COMPLETE
- **Function**: `WndProc` WM_KEYDOWN handler (lines 600-630)
- **Keyboard Shortcuts**:
  ```
  Keys 1-5: Switch active panel (sets g_nCurrentPane)
  Space:    Toggle play/stop (xor g_bPlaying with 1)
  G:        Cycle through genres (increment g_nCurrentGenre mod 5)
  L:        Set AI model loaded (g_bModelLoaded = 1)
  ```
- **Redraw Trigger**: InvalidateRect after each keypress
- **Status**: Functional but minimal

#### **1.2.8 Status Bar Display** ✅ COMPLETE
- **Function**: `DrawStatusBar` (lines 876-975)
- **Layout**: Dark background bar at y=0-45
- **Display Elements**:
  - BPM value (e.g., "BPM: 128")
  - Current genre name (e.g., "Genre: Techno")
  - Play state ("PLAYING" or "STOPPED")
  - Current step counter (e.g., "Step: 5/16")
  - Help text: "Keys: 1-5=Panels, Space=Play, G=Genre"
  - AI model status: "AI Model: Ready" or "AI Model: Not Loaded"
- **Text Color**: #E0E0E0 (light gray)
- **Formatting**: Uses `wsprintfA` with szStatusFormat template
- **Status**: Fully implemented

#### **1.2.9 Audio Output System** ✅ COMPLETE
- **Function**: `InitializeAudio` (lines 2300+), `TriggerAudioBuffer`
- **API**: Windows waveOut (low-level audio streaming)
- **Format**: 
  - PCM 16-bit
  - 2 channels (stereo)
  - 44.1 kHz sample rate
- **Double Buffering**:
  - `g_AudioBuffer1` and `g_AudioBuffer2` (4410 samples each)
  - `g_WaveHdr1` and `g_WaveHdr2` (WAVEHDR structures)
  - Ping-pong: generates audio in one while playing the other
- **Playback Loop**:
  - Timer fires every `g_dwMsPerStep`
  - Calls `TriggerAudioBuffer` if `g_bPlaying`
  - Generates 4410 samples (~100ms)
  - Queues buffer to waveOut via `waveOutWrite`
- **Status**: Production-quality audio delivery

---

### 1.3 MISSING/INCOMPLETE GUI COMPONENTS

#### ❌ **Transport Controls (Play/Stop/Record Buttons)**
- **Current State**: Play/stop toggled by spacebar only, no visual buttons
- **What's Needed**:
  - Three button regions: Play (x=0-120), Stop (x=130-250), Record (x=260-380)
  - Each button: 40px tall, y=5
  - States: Released (background), Pressed (lighter), Hover (highlighted), Active (green/red)
  - Mouse interaction: Click detection, visual feedback
  - Integration: Button presses set `g_bPlaying` and `g_bRecording` flags
- **Effort**: 2-3 hours (button rendering + interaction)

#### ❌ **Mixer Faders & Level Meters**
- **Current State**: `g_fMixerVol0-7` and `g_fMixerPan0-7` exist but no visual UI
- **What's Needed**:
  - Mixer panel (x=1102-1400): 8 vertical track strips
  - Each strip:
    - Volume fader: x=130-170 (relative to strip start), y=80-250 (170px travel)
    - Pan knob/slider: x=180-220, y=100-150
    - VU Meter: 3-segment (peak/RMS/LUFS), x=50-120
    - Mute/Solo buttons: x=225-295, y=80-120
    - Channel label: x=10, y=280
  - Mouse interaction: Drag faders, click buttons
  - Real-time level feedback from audio buffer
- **Effort**: 4-5 hours (complex multi-track layout + interaction)

#### ❌ **EQ Curve Editor**
- **Current State**: `g_fSVF_Cutoff`, `g_fSVF_Resonance` exist but no visual editor
- **What's Needed**:
  - Frequency response curve visualization
  - 3-band EQ (low/mid/high) with adjustable Q
  - Bezier curve rendering through frequency points
  - Mouse interaction: Drag points on curve to adjust gain/frequency
  - Parameter display: Frequency (Hz), Gain (dB), Q factor
  - Real-time filter coefficient updates
- **Effort**: 5-6 hours (curve math + interaction + rendering)

#### ❌ **Effects Rack UI**
- **Current State**: DSP chain (`ApplySVFilter`, `ApplyDistortion`, etc.) works but no UI controls
- **What's Needed**:
  - 4 effect slots visual representation
  - Each slot:
    - Effect name (Distortion, Delay, Reverb, Compression)
    - Bypass toggle button
    - Parameter knobs (visual circles + value displays)
    - Parameter ranges: e.g., Drive 0-100%, Mix 0-100%, Decay 0-1.0
  - Mouse interaction: Click knobs, drag to adjust, right-click for preset menu
  - Real-time parameter binding to `g_fDist*`, `g_fDelay*`, `g_fReverb*`, `g_fComp*`
- **Effort**: 5-6 hours (knob rendering + multi-parameter control)

#### ❌ **Piano Roll Note Editing**
- **Current State**: Piano Roll panel exists but no note editing
- **What's Needed**:
  - Grid overlay: 12 semitones (vertical) × 16 beats (horizontal)
  - Each cell represents one note
  - Interaction:
    - Click cell to place note with default velocity (127)
    - Drag right edge to extend note length
    - Drag vertically to change pitch
    - Right-click to delete or set velocity
    - Ctrl+drag for multi-select and move
  - Visual feedback: Note bars with velocity displayed as opacity/brightness
  - Data structure: `g_PianoRollData` (4096 bytes, 128 notes × 16 steps × 2 bytes)
  - Integration: Trigger synth voices from note data during playback
- **Effort**: 6-8 hours (complex multi-parameter interaction)

#### ❌ **Automation Curve Editing**
- **Current State**: No automation support
- **What's Needed**:
  - Automation lane below each track (in Playlist or dedicated view)
  - Curve editor: Click-drag to place nodes, smooth Bezier interpolation
  - Supported targets: Filter cutoff, Reverb mix, Compressor threshold, Pan
  - Visual: Curve line with node circles, active target highlighted
  - Interaction: Click to add node, drag to move, right-click to delete
  - Real-time: Read automation values during playback and apply to DSP params
- **Effort**: 6-8 hours (curve math + real-time integration)

#### ❌ **File Browser / Pattern Loader**
- **Current State**: Browser panel exists but empty
- **What's Needed**:
  - Tree view: Patterns, Presets, Samples folders
  - Each item: name, size, preview metadata
  - Interaction:
    - Double-click to load pattern into sequencer
    - Right-click menu: Open, Rename, Delete, Duplicate, Properties
    - Drag-drop support for file import
  - Backend: Directory enumeration, file I/O, serialization
  - Display: Scrollable list with icons
- **Effort**: 6-7 hours (file I/O + UI rendering + interaction)

#### ❌ **Menu System**
- **Current State**: No menu bar
- **What's Needed**:
  - Top menu bar: File, Edit, View, Tools
  - File menu: New, Open, Save, Save As, Export, Exit
  - Edit menu: Undo, Redo, Copy, Paste, Select All, Delete
  - View menu: Zoom In, Zoom Out, Toggle panels, Theme, Full-screen
  - Tools menu: Settings, About, Check for Updates
  - Keyboard shortcuts: Show in menu items
  - State management: Grayed-out items when unavailable (e.g., Undo when stack empty)
- **Effort**: 4-5 hours (menu rendering + keyboard routing)

#### ❌ **Context Menus**
- **Current State**: No right-click menus
- **What's Needed**:
  - Sequencer grid context menu: Clear pattern, Load preset, Copy/Paste, Randomize
  - Mixer context menu: Reset channel, Load effect, Duplicate track
  - Piano roll context menu: Cut/Copy/Paste notes, Delete, Select all, Quantize
  - File browser context menu: Open, Rename, Delete, Properties
  - Menu positioning: Below cursor, avoid off-screen
- **Effort**: 4-5 hours (menu layout + positioning + interaction)

#### ❌ **Theme / Color System**
- **Current State**: Hardcoded colors (#202020, #FF6B35, #E0E0E0)
- **What's Needed**:
  - Define theme constants at top of file (dark, light, neon, minimal)
  - Each theme: palette of 10-15 colors (background, text, accent, grid, waveform, etc.)
  - Toggle theme at runtime via menu: View → Theme
  - High-DPI awareness: Scale brush widths, font sizes by screen DPI
  - Persistent: Save selected theme to .ini file
- **Effort**: 2-3 hours (data structure + rendering updates)

#### ❌ **Tooltip / Help System**
- **Current State**: Status bar shows generic help text only
- **What's Needed**:
  - Context-sensitive tooltips on mouse hover (0.5s delay)
  - Tooltips show: Control name, keyboard shortcut, current value, parameter range
  - Tooltip positioning: 10px below cursor, avoid off-screen
  - Fallback text: "Hold Shift for extended help"
  - Integration: Every control has associated tooltip text
- **Effort**: 2-3 hours (timer-based display + positioning)

#### ❌ **Settings / Preferences Dialog**
- **Current State**: No settings UI
- **What's Needed**:
  - Modal dialog with tabs:
    - Audio: Device selection, Latency (buffer size), Sample rate
    - MIDI: Input device, Input channel, MIDI learn mode
    - Keyboard: Keyboard shortcut customization (rebindable)
    - Theme: Color theme selection, UI scale
    - General: BPM default, Snap to grid, Undo history size
  - Buttons: OK, Cancel, Restore Defaults
  - Persistence: Save settings to `underground_king.ini`
- **Effort**: 5-6 hours (dialog layout + file I/O)

#### ❌ **Spectrum Analyzer Visualization**
- **Current State**: Oscilloscope shows waveform, no frequency analysis
- **What's Needed**:
  - Real-time FFT (Fast Fourier Transform) or simplified spectrum
  - Display: 32 frequency bands (log scale, 20Hz-20kHz range)
  - Rendering: Vertical bars with smooth animation
  - Coloring: Color gradient blue→cyan→green→yellow→red based on magnitude
  - Animation: Smooth decay, peak hold option
  - Integration: Update from audio buffer every frame
- **Effort**: 6-7 hours (FFT implementation or library integration)

#### ❌ **Zoom / Pan Controls**
- **Current State**: Fixed 1600×700 window, no zoom
- **What's Needed**:
  - Zoom buttons or keyboard shortcuts: Ctrl+Plus (zoom in), Ctrl+Minus (zoom out)
  - Zoom levels: 50%, 100%, 200%, 400%
  - Pan with arrow keys or middle-mouse drag
  - Visual zoom indicator: Display current zoom % in status bar
  - Scroll bar indicators for panned view
  - Repaint affected regions at new zoom level
- **Effort**: 4-5 hours (coordinate transformation + viewport management)

#### ❌ **Multi-Select & Grouping**
- **Current State**: Single-click cell toggle only
- **What's Needed**:
  - Click-drag to select region (bounding box)
  - Shift+Click to extend selection
  - Ctrl+A to select all
  - Visual feedback: Selected items highlighted
  - Group operations: Move selected items (arrow keys), Delete (Del), Copy (Ctrl+C), Paste (Ctrl+V)
  - Data structures: Selection state tracking
- **Effort**: 3-4 hours (selection logic + rendering)

#### ❌ **Drag-Drop File Operations**
- **Current State**: No file drag-drop support
- **What's Needed**:
  - Register window for OLE drag-drop (COM)
  - Accept drops: .wav, .aiff, .pattern files
  - Drop destinations: Browser (import sample), Mixer (load effect), Timeline (insert pattern)
  - Visual feedback: Drag over changes cursor/highlight
  - Backend: File parsing, format detection, loading into appropriate structure
- **Effort**: 5-6 hours (COM setup + file format parsing)

#### ❌ **MIDI Learn UI**
- **Current State**: No MIDI support
- **What's Needed**:
  - "MIDI Learn Mode" toggle in menu or toolbar
  - User clicks control (e.g., fader) → UI highlights waiting for input
  - User moves MIDI controller (e.g., slider) → Binding saved
  - Visual feedback: Blinking border on control during learn
  - Display binding: "MIDI CC 7" next to control
  - Unbind: Right-click menu option
  - Persistence: Save MIDI map to file
- **Effort**: 4-5 hours (MIDI input handling + binding storage)

#### ❌ **Undo/Redo Visual Feedback**
- **Current State**: No undo/redo system
- **What's Needed**:
  - Undo stack: Track state changes (grid pattern, mixer settings, automation points)
  - Visual history: Small thumbnails or text list of recent actions
  - Interaction: Click thumbnail to jump to state, or use Ctrl+Z/Ctrl+Y
  - Display: Grayed-out undo/redo buttons when stacks empty
  - Max history: Store last 50 states
- **Effort**: 6-8 hours (state serialization + stack management + rendering)

#### ❌ **Project Management**
- **Current State**: No save/load functionality
- **What's Needed**:
  - Project file format: Binary or JSON containing:
    - All pattern data
    - Mixer settings (volumes, pans, effects params)
    - Automation curves
    - File references (loaded samples, presets)
    - Metadata (name, created date, BPM)
  - File I/O: Save to `projects/*.ukp` (Underground King Project)
  - Auto-save: Every 30 seconds if modified
  - Recovery: Detect crash, offer to recover last auto-save
  - Recent projects: Menu showing recently opened projects
- **Effort**: 8-10 hours (file format design + serialization + recovery)

---

## PART 2: IMPLEMENTATION ROADMAP

### **Phase 1: Core Visual Controls (Weeks 1-2)**
Priority order to establish basic professional look:

1. **Transport Controls** (2-3 hrs)
   - Visual Play/Stop/Record buttons
   - Connects to existing `g_bPlaying` flag
   - Quick win: Spacebar already works

2. **Master Volume/Pan Faders** (2 hrs)
   - Simple horizontal sliders in status bar
   - Direct connection to `g_fMasterVolume`, `g_fMixerPan0`
   - Real-time audio feedback

3. **Mixer Track Strips** (4-5 hrs)
   - 8 vertical faders (one per channel)
   - Basic VU meter display
   - Mute/Solo buttons
   - High visual impact

4. **Theme/Color System** (2 hrs)
   - Extract hardcoded colors to constants
   - Implement dark/light theme toggle
   - Set up high-DPI scaling

### **Phase 2: Frequency-Domain Controls (Weeks 2-3)**
Building on visual foundation:

5. **EQ Curve Editor** (5-6 hrs)
   - 3-band parametric EQ visual
   - Frequency curve rendering
   - Parameter knobs for freq/gain/Q

6. **Effects Rack UI** (5-6 hrs)
   - Visual representation of 4 effect slots
   - Bypass toggles
   - Parameter knobs for each effect
   - Real-time control binding

### **Phase 3: Time-Domain Editing (Weeks 3-4)**
Complex multi-parameter interfaces:

7. **Piano Roll Note Editing** (6-8 hrs)
   - Interactive note grid
   - Drag-resize note length
   - Velocity display/editing
   - Real-time synth triggering

8. **Automation Curve Editing** (6-8 hrs)
   - Bezier curve nodes
   - Smooth interpolation
   - Real-time parameter modulation

### **Phase 4: Navigation & File Handling (Week 4-5)**
File system and workflow:

9. **File Browser** (6-7 hrs)
   - Directory tree view
   - Pattern/preset loading
   - Context menus

10. **Menu System** (4-5 hrs)
    - File/Edit/View/Tools menus
    - Keyboard shortcuts
    - State management

11. **Zoom/Pan Controls** (4-5 hrs)
    - Zoom in/out
    - Pan navigation
    - Viewport management

### **Phase 5: Polish & Integration (Week 5-6)**
Final touches and quality:

12. **Context Menus** (4-5 hrs)
    - Right-click workflows
    - Quick access to common operations

13. **Tooltip/Help System** (2-3 hrs)
    - Hover tooltips
    - Status bar help

14. **Settings Dialog** (5-6 hrs)
    - Audio device selection
    - Theme/appearance options
    - Keyboard customization

15. **Multi-Select & Grouping** (3-4 hrs)
    - Selection box rendering
    - Group operations

16. **Spectrum Analyzer** (6-7 hrs)
    - FFT visualization
    - Real-time frequency display

### **Phase 6: Advanced Features (Week 6-7)**
Professional workflow features:

17. **Undo/Redo System** (6-8 hrs)
    - State stack management
    - Visual history display

18. **Project Management** (8-10 hrs)
    - Save/load projects
    - Auto-save and recovery

19. **Drag-Drop File Operations** (5-6 hrs)
    - OLE registration
    - File format parsing

20. **MIDI Learn** (4-5 hrs)
    - Controller mapping
    - Visual feedback

### **Phase 7: Polish & Testing (Week 7)**

21. **Tooltip/Help System** (2-3 hrs)
22. **Theme Refinement** (2-3 hrs)
23. **Integration Testing** (4-5 hrs)
24. **Performance Optimization** (3-4 hrs)

---

## PART 3: COMPONENT SPECIFICATIONS

### Component #1: Transport Controls

**Location**: Top status bar (x=0-380, y=5-45)

**Buttons**:
```
Play:   x=0,   w=110  → Icon: ▶ (triangle)
Stop:   x=120, w=110  → Icon: ■ (square)
Record: x=240, w=110  → Icon: ● (circle, red)
```

**States per Button**:
```
Released: Background = #303030, Border = #E0E0E0
Hover:    Background = #404040, Border = #FF6B35
Pressed:  Background = #FF6B35, Text = #000000
Active:   Solid highlight color (green for Play, red for Record)
```

**Mouse Interaction**:
```
WM_LBUTTONDOWN:
  - Check x position, determine button
  - Call appropriate handler:
    - Play   → g_bPlaying = 1
    - Stop   → g_bPlaying = 0
    - Record → g_bRecording ^= 1 (toggle)
  - InvalidateRect to redraw

WM_MOUSEMOVE:
  - Determine if hovering over button
  - Update button state for visual feedback
```

**Implementation Strategy**:
1. Define button rectangles as constants
2. Add button state variables (`g_nPlayState`, `g_bRecordActive`)
3. Draw buttons in `DrawStatusBar` using `Rectangle` and `TextOutA`
4. Update `HandleGridClick` to check status bar region
5. Connect button presses to existing audio control logic

---

### Component #2: Mixer Track Strips

**Layout**: Mixer panel (x=1102-1400, y=80-650), 8 tracks

**Per-Track Strip (width=36px)**:
```
Track Layout (x varies, y fixed):
  ┌─────────────────────────────┐
  │ CH 1                        │ y=80 (label)
  ├─────────────────────────────┤
  │ ▔▔▔ Volume Fader ▔▔▔        │ y=110-280 (170px height)
  │  0dB                        │
  │                             │
  │  -6dB                       │
  │                             │
  │  -∞dB ▲                     │ y=280 (fader thumb)
  ├─────────────────────────────┤
  │ │    VU Meter    │          │ y=300-340
  │ │ ███░░░░░░░░ 0dB│          │
  │ ├─────────────────┤         │
  │ │ ███░░░░░░░░ 0dB│ (RMS)    │
  │ ├─────────────────┤         │
  │ │ ░░░░░░░░░░░░ -∞│ (Peak)   │
  ├─────────────────────────────┤
  │ [M] [S]                     │ y=350 (Mute, Solo buttons)
  └─────────────────────────────┘
```

**Data Structures**:
```asm
; In .data section
g_fMixerVol0-7      real4 (8 × 4 bytes = 32 bytes) [0.0-1.0]
g_fMixerPan0-7      real4 (8 × 4 bytes = 32 bytes) [-1.0 to 1.0]
g_dwPeakLevel       dd (updated from audio buffer)
g_fRMSLevel         real4
g_nMixerMute        db 8 dup(0) [0=unmuted, 1=muted]
g_nMixerSolo        db 8 dup(0) [0=not soloed, 1=soloed]
g_nHoverTrack       dd -1 [currently hovered track, -1 = none]
```

**Rendering** (`DrawMixerPanel` function):
```
For each of 8 tracks:
  1. Draw track background rectangle
  2. Draw volume fader:
     - Draw slider track (3px wide, light gray)
     - Calculate thumb position: y = 280 - (g_fMixerVol * 170)
     - Draw thumb (12px × 12px box, orange if g_nHoverTrack matches)
  3. Draw VU meters (3 segments, color gradient)
  4. Draw mute/solo buttons
  5. Draw channel label and current level text
```

**Interaction** (`HandleMixerClick` function):
```
WM_LBUTTONDOWN in mixer region:
  1. Calculate track: (x - 1102) / 36 → track_index
  2. Determine control within track:
     - y ∈ [110, 280]: Volume fader
       - Set g_fMixerVol[track] = (280 - y) / 170 [clamped 0-1]
     - y ∈ [350, 370]: Mute button
       - Toggle g_nMixerMute[track]
     - y ∈ [380, 400]: Solo button
       - Set all g_nMixerSolo to 0, then g_nMixerSolo[track] = 1
  3. InvalidateRect to redraw

WM_MOUSEMOVE in mixer region:
  - Update g_nHoverTrack for visual feedback
```

**Audio Integration**:
```
In GenerateAudioBuffer:
  - Check g_nMixerMute[channel] before adding to mix
  - Apply g_fMixerVol[channel] gain to each synth output
  - Monitor peak level, update g_dwPeakLevel for meter display
```

---

### Component #3: EQ Curve Editor

**Location**: Playlist panel (x=702-1100, y=100-300) – overlays sequencer

**Layout**:
```
Frequency Response Curve Editor
┌──────────────────────────────────────────┐
│ Low     Mid     High                     │ (Band labels)
│                                          │
│     ╭─╮                                  │
│ ──┤ │  ├──                               │ (Curve with 3 control points)
│   ╰─╯                                    │
│                                          │
│ Hz: 200  │  Q: 1.2  │  Gain: +6dB        │ (Parameter display)
└──────────────────────────────────────────┘
```

**Data Structures**:
```asm
; EQ band parameters (3 bands: Low, Mid, High)
g_nEQ_BandCount     dd 3
g_fEQ_Freq0         real4 200.0    ; Low band center frequency (Hz)
g_fEQ_Gain0         real4 0.0      ; Low band gain (dB, -24 to +24)
g_fEQ_Q0            real4 0.707    ; Low band Q factor

g_fEQ_Freq1         real4 2000.0   ; Mid band
g_fEQ_Gain1         real4 0.0
g_fEQ_Q1            real4 0.707

g_fEQ_Freq2         real4 8000.0   ; High band
g_fEQ_Gain2         real4 0.0
g_fEQ_Q2            real4 0.707

g_nEQ_DragBand      dd -1          ; Currently dragging band (-1 = none)
g_nEQ_DragParam     dd 0           ; 0=freq, 1=gain, 2=Q
```

**Rendering** (`DrawEQEditor` function):
```
1. Draw background rectangle
2. Draw frequency axis labels (20Hz, 100Hz, 1kHz, 10kHz, 20kHz)
3. Draw gain grid lines (±24dB, ±12dB, 0dB)
4. Calculate curve points:
   For each band:
     - x_pos = Map frequency to pixel (log scale, 20Hz-20kHz → x=0-398)
     - y_pos = Map gain to pixel (0dB center, ±24dB range → y±100)
     - Draw control point circle (r=6px, orange if dragging)
5. Draw Bezier curve connecting 3 points + endpoints (flat at edges)
6. Draw parameter display text below curve
```

**Interaction** (`HandleEQInteraction` function):
```
WM_LBUTTONDOWN in EQ area:
  1. Check if clicking on band control point (within 10px circle)
     If yes:
       - Set g_nEQ_DragBand = band_index
       - Wait for WM_MOUSEMOVE
  2. If not on control point but in editor area:
     - Check shift key for adding new band
     - Or select band for keyboard parameter adjustment

WM_MOUSEMOVE (if dragging):
  1. Calculate new parameter value from cursor position
  2. Clamp to valid range
  3. Update g_fEQ_Freq[band] or g_fEQ_Gain[band] or g_fEQ_Q[band]
  4. Update SVF filter coefficients in real-time
  5. InvalidateRect to redraw

WM_LBUTTONUP:
  - Set g_nEQ_DragBand = -1
  - Finalize parameter updates
```

**SVF Filter Integration**:
```
Per audio frame:
  - Calculate SVF coefficients from g_fEQ_Gain0-2, g_fEQ_Freq0-2, g_fEQ_Q0-2
  - For 3-band parametric: cascade 3 peaking filters
  - Apply to audio buffer during ApplySVFilter
```

---

## PART 4: AUDIO ARCHITECTURE NOTES

### Real-Time Audio Loop
```
Main Thread:
  └─ WM_TIMER (g_dwMsPerStep interval)
       └─ if g_bPlaying:
            └─ GenerateAudioBuffer
                 ├─ Iterate pattern data for current step
                 ├─ Trigger synth voices (kick, snare, hihat, bass)
                 ├─ Generate 4410 samples (~100ms at 44.1kHz)
                 └─ ApplyDSPChain (SVF → Distortion → Delay → Reverb → Compression)
            └─ waveOutWrite (queue buffer to audio device)
            └─ Increment g_nCurrentStep (mod 16)
```

### Synthesis Models Currently Implemented
- **Kick**: Sine oscillator with exponential envelope decay
- **Snare**: Dual sine + filtered noise with ADSR envelope
- **Hihat**: Noise generator with amplitude envelope
- **Bass**: Sine oscillator with exponential decay
- **Available (not used yet)**:
  - FM Synthesis (modulator + carrier phases)
  - Wavetable Synthesis (pre-computed wave lookup)
  - Subtractive Lead Synth (ADSR envelope + filter)

### DSP Effects Chain (In Application Order)
1. **SVF Filter**: Multimode (low/band/high/notch) 12dB/octave
2. **Distortion**: Soft clipping with drive and wet/dry mix
3. **Delay**: Feedback echo (up to 1 second buffer)
4. **Reverb**: Schroeder architecture (4 parallel delay lines + damping)
5. **Compressor**: Dynamic range with envelope follower + attack/release

All effects have parameters that can be controlled via UI (once UI is built).

---

## PART 5: BUILDING THE GUI - TECHNICAL PATTERNS

### Mouse Event Handling Pattern
```asm
@@wm_lbuttondown:
    mov r9, [rbp+28h]           ; Get lParam (packed x,y)
    
    ; Extract X and Y coordinates
    movzx ecx, ax               ; X coordinate (low word)
    shr rax, 16
    movzx edx, ax               ; Y coordinate (high word)
    
    ; Determine which control was clicked
    cmp ecx, REGION_X_START
    jl @@not_in_region
    cmp ecx, REGION_X_END
    jg @@not_in_region
    cmp edx, REGION_Y_START
    jl @@not_in_region
    cmp edx, REGION_Y_END
    jg @@not_in_region
    
    ; Handle click in region
    mov eax, ecx
    sub eax, REGION_X_START
    mov ecx, CONTROL_WIDTH
    xor edx, edx
    div ecx                    ; eax = control_index
    
    ; Process based on control_index
    ...
    
    InvalidateRect              ; Redraw
```

### Fader Rendering Pattern
```asm
; Input: ecx = x_position, edx = y_position, xmm0 = fader_value (0-1)
DrawFader proc
    ; Draw track (vertical line)
    mov rcx, g_hDC
    mov edx, x_pos
    mov r8d, y_track_start
    call MoveToEx
    
    mov rcx, g_hDC
    lea rdx, [x_pos]
    mov r8d, y_track_end
    call LineTo
    
    ; Calculate thumb position
    ; thumb_y = y_start + (1 - value) * travel_distance
    movss xmm1, xmm0            ; fader value
    mov eax, 3F800000h          ; 1.0
    movd xmm2, eax
    subss xmm2, xmm1            ; (1 - value)
    mov eax, FADER_HEIGHT
    cvtsi2ss xmm3, eax
    mulss xmm2, xmm3            ; (1 - value) * height
    cvtss2si eax, xmm2
    add eax, FADER_Y_START
    mov thumb_y, eax
    
    ; Draw thumb (small rectangle)
    mov dword ptr [rect+0], x_pos - THUMB_WIDTH/2
    mov dword ptr [rect+4], thumb_y - THUMB_HEIGHT/2
    mov dword ptr [rect+8], x_pos + THUMB_WIDTH/2
    mov dword ptr [rect+12], thumb_y + THUMB_HEIGHT/2
    
    mov rcx, g_hDC
    lea rdx, [rect]
    mov r8, hover_brush        ; Orange if hovered, gray otherwise
    call FillRect
    
    ret
DrawFader endp
```

### Coordinate Mapping for Complex Layouts
```asm
; General pattern for multi-region UI
GetControlFromCoordinates proc
    ; rcx = x, rdx = y
    ; returns: eax = control_id (or -1 if none)
    
    ; Check each region in priority order
    
    ; Region 1: Sequencer grid (high priority, frequently used)
    cmp ecx, GRID_X_MIN
    jl @@check_region2
    cmp ecx, GRID_X_MAX
    jg @@check_region2
    cmp edx, GRID_Y_MIN
    jl @@check_region2
    cmp edx, GRID_Y_MAX
    jg @@check_region2
    ; Inside grid - calculate cell
    mov eax, ecx
    sub eax, GRID_X_MIN
    mov ecx, CELL_WIDTH
    xor edx, edx
    div ecx
    mov [grid_step], eax
    ... similar for row ...
    mov eax, CONTROL_GRID
    ret
    
@@check_region2:
    ; Region 2: Mixer faders
    cmp ecx, MIXER_X_MIN
    ...
    
@@not_found:
    mov eax, -1
    ret
GetControlFromCoordinates endp
```

---

## PART 6: ESTIMATED TIME BREAKDOWN

| Component | Complexity | Estimated Hours | Effort Level |
|-----------|-----------|-----------------|--------------|
| Transport Controls | Low | 2-3 | ★★☆ |
| Master Vol/Pan | Low | 2 | ★☆☆ |
| Mixer Faders | Medium | 4-5 | ★★★ |
| EQ Curve Editor | High | 5-6 | ★★★★ |
| Effects Rack | High | 5-6 | ★★★★ |
| Piano Roll | Very High | 6-8 | ★★★★★ |
| Automation | Very High | 6-8 | ★★★★★ |
| File Browser | High | 6-7 | ★★★★ |
| Menu System | Medium | 4-5 | ★★★ |
| Context Menus | Medium | 4-5 | ★★★ |
| Theme System | Low | 2-3 | ★★☆ |
| Tooltips | Low | 2-3 | ★★☆ |
| Settings Dialog | Medium | 5-6 | ★★★ |
| Spectrum Analyzer | High | 6-7 | ★★★★ |
| Zoom/Pan | Medium | 4-5 | ★★★ |
| Multi-Select | Low-Med | 3-4 | ★★☆ |
| Drag-Drop | High | 5-6 | ★★★★ |
| MIDI Learn | Medium | 4-5 | ★★★ |
| Undo/Redo | Very High | 6-8 | ★★★★★ |
| Project Management | Very High | 8-10 | ★★★★★ |
| **TOTAL** | | **110-145 hours** | |

**Estimated Duration**: 6-8 weeks at 20 hours/week, or 4-5 weeks at 30 hours/week

---

## PART 7: QUALITY ASSURANCE CHECKLIST

Before declaring each component "complete":

- [ ] Visual rendering is pixel-perfect and high-DPI aware
- [ ] Mouse interaction is responsive (no lag, proper hit detection)
- [ ] Keyboard shortcuts work as documented
- [ ] Real-time audio updates reflect UI changes immediately
- [ ] No memory leaks (test with long sessions)
- [ ] Graceful handling of edge cases (min/max values, overflow)
- [ ] Color contrast meets WCAG accessibility standards
- [ ] Tooltip text is clear and helpful
- [ ] Performance: 60 FPS UI updates, audio buffer generation < 5% CPU

---

## CONCLUSION

omega.asm provides a solid foundation with working audio synthesis, DSP effects, and basic UI panels. The roadmap above transforms it into a professional-grade DAW through systematic addition of interactive GUI components, organized into logical phases that balance visual impact with technical complexity.

**Next immediate action**: Start with Phase 1 (Transport Controls + Master Faders) to establish the basic visual vocabulary and interaction patterns for the entire interface.

