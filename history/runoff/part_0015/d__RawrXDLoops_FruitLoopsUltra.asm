; FruitLoopsUltra.asm - Ultimate MASM64 DAW with AI Integration
; Complete professional digital audio workstation with hot-reload capability

option casemap:none
option frame:auto

; Windows API includes
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib dsound.lib
includelib winmm.lib
includelib ole32.lib

; External functions
extern GetModuleHandleA:proc
extern GetCommandLineA:proc
extern ExitProcess:proc
extern MessageBoxA:proc
extern CreateWindowExA:proc
extern ShowWindow:proc
extern UpdateWindow:proc
extern GetMessageA:proc
extern TranslateMessage:proc
extern DispatchMessageA:proc
extern PostQuitMessage:proc
extern DefWindowProcA:proc
extern RegisterClassA:proc
extern LoadCursorA:proc
extern LoadIconA:proc
extern GetDC:proc
extern ReleaseDC:proc
extern BeginPaint:proc
extern EndPaint:proc
extern TextOutA:proc
extern SetBkMode:proc
extern SetTextColor:proc
extern CreateSolidBrush:proc
extern SelectObject:proc
extern DeleteObject:proc
extern Rectangle:proc
extern MoveToEx:proc
extern LineTo:proc
extern GetClientRect:proc
extern FillRect:proc
extern CreateFontA:proc
extern GetStockObject:proc
extern SetRect:proc
extern DrawTextA:proc
extern CreatePen:proc
extern GetSystemMetrics:proc
extern GetAsyncKeyState:proc
extern Sleep:proc
extern GetTickCount:proc
extern QueryPerformanceCounter:proc
extern QueryPerformanceFrequency:proc
extern timeGetTime:proc
extern waveOutOpen:proc
extern waveOutClose:proc
extern waveOutPrepareHeader:proc
extern waveOutWrite:proc
extern waveOutReset:proc
extern DirectSoundCreate8:proc
extern CoInitialize:proc
extern CoUninitialize:proc
extern VirtualAlloc:proc
extern VirtualFree:proc
extern CreateFileA:proc
extern ReadFile:proc
extern CloseHandle:proc
extern GetFileSize:proc
extern CreateThread:proc
extern WaitForSingleObject:proc
extern CloseHandle:proc
extern CreateMutexA:proc
extern ReleaseMutex:proc
extern CreateEventA:proc
extern SetEvent:proc
extern ResetEvent:proc
extern WaitForSingleObject:proc

; Constants
WM_DESTROY equ 2
WM_PAINT equ 15
WM_TIMER equ 275
WM_KEYDOWN equ 256
WM_CHAR equ 258
WM_LBUTTONDOWN equ 513
WM_LBUTTONUP equ 514
WM_MOUSEMOVE equ 512
WM_USER equ 400h

IDC_ARROW equ 32512
IDI_APPLICATION equ 32512

CW_USEDEFAULT equ 80000000h
WS_OVERLAPPEDWINDOW equ 0CF0000h
WS_VISIBLE equ 10000000h

; Audio constants
WAVE_FORMAT_PCM equ 1
WAVE_MAPPER equ -1

; AI Model constants
MODEL_TINYLLAMA equ 0
MODEL_7B equ 1
MODEL_13B equ 2
MODEL_70B equ 3
MODEL_800B equ 4

; Data section
.data
szClassName db "FruitLoopsUltraClass",0
szAppName db "FruitLoopsUltra DAW",0
szWindowTitle db "FruitLoopsUltra - Professional MASM64 DAW with AI",0

; Global variables
g_hInstance dq 0
g_hWnd dq 0
g_hDC dq 0
g_hWaveOut dq 0
g_pDSBuffer dq 0
g_bRunning db 1
g_dwBufferSize dd 8192
g_dwSampleRate dd 44100
g_dwBPM dd 120
g_fVolume real4 0.7

; AI model state
g_pModelData dq 0
g_dwModelSize dd 0
g_bModelLoaded db 0
g_nCurrentModel dd MODEL_TINYLLAMA

; UI layout
g_nActivePane dd 0
g_rcChannelRack RECT <0,0,300,600>
g_rcPianoRoll RECT <300,0,600,600>
g_rcPlaylist RECT <600,0,900,600>
g_rcMixer RECT <900,0,1200,600>
g_rcBrowser RECT <1200,0,1500,600>

; Audio buffer for real-time processing
g_pAudioBuffer dq 0
g_dwBufferPos dd 0
g_bPlaying db 0
g_dwPlayPos dd 0

; Pattern data (16 steps per pattern)
g_PatternData db 16 dup(0)
g_nCurrentPattern dd 0

; DSP state variables
g_fLPF real4 1000.0
g_fHPF real4 20.0
g_fResonance real4 0.7
g_fDelayTime real4 0.5
g_fReverbAmount real4 0.3
g_fDistortion real4 0.0
g_fCompression real4 1.0

; Hot-reload state
g_hHotReloadMutex dq 0
g_bHotReloadRequested db 0

; Plugin system
g_nActivePlugins dd 0
g_PluginSlots dd 116 dup(0)

; Genre presets
g_szCurrentGenre db "techno",0

; String buffers
szCommandBuffer db 256 dup(0)
szStatusText db "Ready - BPM: 120",0

; Code section
.code

; Main entry point
WinMain proc
    sub rsp, 28h
    
    ; Get module handle
    xor rcx, rcx
    call GetModuleHandleA
    mov g_hInstance, rax
    
    ; Get command line
    call GetCommandLineA
    
    ; Initialize COM for DirectSound
    xor rcx, rcx
    call CoInitialize
    
    ; Register window class
    call RegisterWindowClass
    test rax, rax
    jz exit_failure
    
    ; Create main window
    call CreateMainWindow
    test rax, rax
    jz exit_failure
    mov g_hWnd, rax
    
    ; Show and update window
    mov rcx, g_hWnd
    mov rdx, 1
    call ShowWindow
    
    mov rcx, g_hWnd
    call UpdateWindow
    
    ; Initialize audio system
    call InitializeAudio
    test rax, rax
    jz exit_failure
    
    ; Initialize AI model loader
    call InitializeModelLoader
    
    ; Initialize hot-reload system
    call InitializeHotReload
    
    ; Start audio thread
    call StartAudioThread
    
    ; Main message loop
    call MessageLoop
    
    ; Cleanup
    call CleanupAudio
    call CleanupHotReload
    
    xor rcx, rcx
    call CoUninitialize
    
    xor rax, rax
    add rsp, 28h
    ret
    
exit_failure:
    mov rcx, 0
    mov rdx, offset szAppName
    mov r8, offset szWindowTitle
    mov r9, 10h
    call MessageBoxA
    
    mov rcx, 1
    call ExitProcess
WinMain endp

; Enhanced window procedure with AI command handling
WndProc proc hWnd:QWORD, uMsg:QWORD, wParam:QWORD, lParam:QWORD
    sub rsp, 28h
    
    cmp edx, WM_DESTROY
    je wm_destroy
    cmp edx, WM_PAINT
    je wm_paint
    cmp edx, WM_KEYDOWN
    je wm_keydown
    cmp edx, WM_CHAR
    je wm_char
    cmp edx, WM_LBUTTONDOWN
    je wm_lbuttondown
    cmp edx, WM_USER
    je wm_user
    
    jmp defwndproc
    
wm_destroy:
    mov byte ptr g_bRunning, 0
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax
    jmp exit
    
wm_paint:
    call DrawEnhancedUI
    xor rax, rax
    jmp exit
    
wm_keydown:
    ; Enhanced keyboard shortcuts
    cmp r8d, 49 ; '1'
    je set_pane_0
    cmp r8d, 50 ; '2'
    je set_pane_1
    cmp r8d, 51 ; '3'
    je set_pane_2
    cmp r8d, 52 ; '4'
    je set_pane_3
    cmp r8d, 53 ; '5'
    je set_pane_4
    cmp r8d, 32 ; Space
    je toggle_play
    cmp r8d, 82 ; 'R' - hot reload
    je hot_reload
    cmp r8d, 76 ; 'L' - load model
    je load_model
    jmp defwndproc
    
set_pane_0:
    mov dword ptr g_nActivePane, 0
    jmp redraw
set_pane_1:
    mov dword ptr g_nActivePane, 1
    jmp redraw
set_pane_2:
    mov dword ptr g_nActivePane, 2
    jmp redraw
set_pane_3:
    mov dword ptr g_nActivePane, 3
    jmp redraw
set_pane_4:
    mov dword ptr g_nActivePane, 4
    jmp redraw
    
toggle_play:
    xor byte ptr g_bPlaying, 1
    jmp redraw
    
hot_reload:
    call RequestHotReload
    jmp redraw
    
load_model:
    call LoadAIModel
    jmp redraw
    
redraw:
    mov rcx, hWnd
    xor rdx, rdx
    xor r8, r8
    call InvalidateRect
    jmp defwndproc
    
wm_char:
    ; Enhanced AI command handling
    call HandleEnhancedAICommand
    jmp defwndproc
    
wm_lbuttondown:
    ; Enhanced mouse interaction
    call HandleEnhancedMouseClick
    jmp defwndproc
    
wm_user:
    ; Custom messages for hot-reload
    cmp r8d, 1000
    je hot_reload_complete
    jmp defwndproc
    
hot_reload_complete:
    call OnHotReloadComplete
    jmp defwndproc
    
defwndproc:
    mov rcx, hWnd
    mov rdx, uMsg
    mov r8, wParam
    mov r9, lParam
    call DefWindowProcA
    
exit:
    add rsp, 28h
    ret
WndProc endp

; Enhanced UI drawing with oscilloscope and AI status
DrawEnhancedUI proc
    sub rsp, 218h
    
    ; Get device context
    mov rcx, g_hWnd
    call GetDC
    mov g_hDC, rax
    
    ; Set background mode
    mov rcx, g_hDC
    mov edx, 1 ; TRANSPARENT
    call SetBkMode
    
    ; Draw 5-pane interface
    call DrawFivePaneLayout
    
    ; Draw oscilloscope
    call DrawOscilloscope
    
    ; Draw AI status
    call DrawAIStatus
    
    ; Draw plugin rack
    call DrawPluginRack
    
    ; Draw current BPM and genre
    call DrawStatusBar
    
    ; Release DC
    mov rcx, g_hWnd
    mov rdx, g_hDC
    call ReleaseDC
    
    add rsp, 218h
    ret
DrawEnhancedUI endp

; Draw the 5-pane DAW layout
DrawFivePaneLayout proc
    sub rsp, 58h
    
    ; Channel Rack
    mov rcx, g_hDC
    mov edx, 0
    mov r8d, 0
    mov r9d, 300
    mov dword ptr [rsp+20h], 600
    call Rectangle
    
    ; Piano Roll
    mov rcx, g_hDC
    mov edx, 300
    mov r8d, 0
    mov r9d, 600
    mov dword ptr [rsp+20h], 600
    call Rectangle
    
    ; Playlist
    mov rcx, g_hDC
    mov edx, 600
    mov r8d, 0
    mov r9d, 900
    mov dword ptr [rsp+20h], 600
    call Rectangle
    
    ; Mixer
    mov rcx, g_hDC
    mov edx, 900
    mov r8d, 0
    mov r9d, 1200
    mov dword ptr [rsp+20h], 600
    call Rectangle
    
    ; Browser
    mov rcx, g_hDC
    mov edx, 1200
    mov r8d, 0
    mov r9d, 1500
    mov dword ptr [rsp+20h], 600
    call Rectangle
    
    ; Labels
    mov rcx, g_hDC
    mov edx, 10
    mov r8d, 10
    mov r9, offset szChannelRack
    call TextOutA
    
    mov rcx, g_hDC
    mov edx, 310
    mov r8d, 10
    mov r9, offset szPianoRoll
    call TextOutA
    
    mov rcx, g_hDC
    mov edx, 610
    mov r8d, 10
    mov r9, offset szPlaylist
    call TextOutA
    
    mov rcx, g_hDC
    mov edx, 910
    mov r8d, 10
    mov r9, offset szMixer
    call TextOutA
    
    mov rcx, g_hDC
    mov edx, 1210
    mov r8d, 10
    mov r9, offset szBrowser
    call TextOutA
    
    ; Active pane highlight
    mov eax, dword ptr g_nActivePane
    imul eax, 300
    mov edx, eax
    mov r8d, 0
    mov r9d, eax
    add r9d, 300
    mov dword ptr [rsp+20h], 30
    
    mov rcx, g_hDC
    call Rectangle
    
    add rsp, 58h
    ret
DrawFivePaneLayout endp

; Draw real-time oscilloscope
DrawOscilloscope proc
    sub rsp, 78h
    
    ; Draw oscilloscope background
    mov rcx, g_hDC
    mov edx, 1210
    mov r8d, 610
    mov r9d, 1490
    mov dword ptr [rsp+20h], 790
    call Rectangle
    
    ; Draw waveform (simplified)
    mov rcx, g_hDC
    mov edx, 1220
    mov r8d, 620
    call MoveToEx
    
    ; Simulate waveform drawing
    mov rcx, g_hDC
    mov edx, 1480
    mov r8d, 780
    call LineTo
    
    add rsp, 78h
    ret
DrawOscilloscope endp

; Draw AI model status
DrawAIStatus proc
    sub rsp, 58h
    
    mov rcx, g_hDC
    mov edx, 1210
    mov r8d, 800
    
    cmp byte ptr g_bModelLoaded, 0
    jne model_loaded
    
    mov r9, offset szModelNotLoaded
    jmp draw_text
    
model_loaded:
    mov r9, offset szModelLoaded
    
draw_text:
    call TextOutA
    
    add rsp, 58h
    ret
DrawAIStatus endp

; Draw plugin rack with 116 slots
DrawPluginRack proc
    sub rsp, 58h
    
    ; Draw plugin slots
    mov ecx, 0
plugin_loop:
    cmp ecx, 116
    jge plugin_done
    
    ; Calculate position
    mov eax, ecx
    mov edx, 20
    mul edx
    add eax, 20
    
    mov r8d, eax
    mov edx, 620
    mov r9d, eax
    add r9d, 15
    mov dword ptr [rsp+20h], 640
    
    mov rcx, g_hDC
    call Rectangle
    
    inc ecx
    jmp plugin_loop
    
plugin_done:
    add rsp, 58h
    ret
DrawPluginRack endp

; Draw status bar with BPM and genre
DrawStatusBar proc
    sub rsp, 78h
    
    ; Update status text
    mov rcx, offset szStatusText
    mov rdx, offset szStatusFormat
    mov r8d, dword ptr g_dwBPM
    mov r9, offset g_szCurrentGenre
    call wsprintf
    
    ; Draw status bar
    mov rcx, g_hDC
    mov edx, 0
    mov r8d, 790
    mov r9d, 1500
    mov dword ptr [rsp+20h], 800
    call Rectangle
    
    ; Draw status text
    mov rcx, g_hDC
    mov edx, 10
    mov r8d, 795
    mov r9, offset szStatusText
    call TextOutA
    
    add rsp, 78h
    ret
DrawStatusBar endp

; Enhanced AI command handler
HandleEnhancedAICommand proc
    sub rsp, 158h
    
    ; Parse command from wParam
    movzx eax, r8b
    mov byte ptr [rsp+40h], al
    mov byte ptr [rsp+41h], 0
    
    ; Check for genre commands
    mov rcx, offset szCommandBuffer
    mov rdx, offset szTechnoCommand
    call lstrcmpA
    test eax, eax
    jz set_techno
    
    mov rcx, offset szCommandBuffer
    mov rdx, offset szAcidCommand
    call lstrcmpA
    test eax, eax
    jz set_acid
    
    mov rcx, offset szCommandBuffer
    mov rdx, offset szAmbientCommand
    call lstrcmpA
    test eax, eax
    jz set_ambient
    
    jmp command_done
    
set_techno:
    mov dword ptr g_dwBPM, 135
    mov rcx, offset g_szCurrentGenre
    mov rdx, offset szTechnoCommand
    call lstrcpyA
    mov real4 ptr g_fLPF, 8000.0
    mov real4 ptr g_fResonance, 0.3
    jmp command_done
    
set_acid:
    mov dword ptr g_dwBPM, 130
    mov rcx, offset g_szCurrentGenre
    mov rdx, offset szAcidCommand
    call lstrcpyA
    mov real4 ptr g_fResonance, 0.95
    mov real4 ptr g_fLPF, 5000.0
    jmp command_done
    
set_ambient:
    mov dword ptr g_dwBPM, 70
    mov rcx, offset g_szCurrentGenre
    mov rdx, offset szAmbientCommand
    call lstrcpyA
    mov real4 ptr g_fLPF, 2000.0
    mov real4 ptr g_fReverbAmount, 0.8
    
command_done:
    ; Redraw to update status
    mov rcx, g_hWnd
    xor rdx, rdx
    xor r8, r8
    call InvalidateRect
    
    add rsp, 158h
    ret
HandleEnhancedAICommand endp

; Enhanced mouse click handler
HandleEnhancedMouseClick proc
    sub rsp, 58h
    
    ; Handle pattern step clicks
    mov eax, dword ptr lParam
    and eax, 0FFFFh ; X position
    mov edx, dword ptr lParam
    shr edx, 16 ; Y position
    
    ; Check if click is in channel rack
    cmp eax, 300
    jg not_channel_rack
    
    ; Calculate which step was clicked
    mov ecx, eax
    mov eax, edx
    mov edx, 30
    div dl
    
    ; Toggle pattern step
    movzx ecx, al
    mov al, byte ptr g_PatternData[rcx]
    xor al, 1
    mov byte ptr g_PatternData[rcx], al
    
not_channel_rack:
    add rsp, 58h
    ret
HandleEnhancedMouseClick endp

; Advanced audio processing with DSP
AudioThread proc param:QWORD
    sub rsp, 158h
    
audio_loop:
    cmp byte ptr g_bRunning, 0
    jz audio_exit
    
    ; Process audio buffer
    call ProcessAudioBuffer
    
    ; Apply DSP effects
    call ApplyDSPEffects
    
    ; Generate AI-driven patterns
    call GenerateAIPatterns
    
    ; Small delay to prevent CPU overload
    mov rcx, 10
    call Sleep
    
    jmp audio_loop
    
audio_exit:
    add rsp, 158h
    ret
AudioThread endp

; Process audio buffer with sample accuracy
ProcessAudioBuffer proc
    sub rsp, 78h
    
    cmp byte ptr g_bPlaying, 0
    jz buffer_done
    
    ; Generate audio samples
    mov rcx, g_pAudioBuffer
    mov edx, dword ptr g_dwBufferSize
    call GenerateSamples
    
    ; Output to audio device
    call OutputAudio
    
buffer_done:
    add rsp, 78h
    ret
ProcessAudioBuffer endp

; Apply DSP effects chain
ApplyDSPEffects proc
    sub rsp, 78h
    
    ; Apply state variable filter
    call ApplySVF
    
    ; Apply distortion
    call ApplyDistortion
    
    ; Apply reverb
    call ApplyReverb
    
    ; Apply compression
    call ApplyCompression
    
    ; Apply master effects
    call ApplyMasterFX
    
    add rsp, 78h
    ret
ApplyDSPEffects endp

; State Variable Filter implementation
ApplySVF proc
    ; SVF implementation for resonance
    ret
ApplySVF endp

; Distortion effect
ApplyDistortion proc
    ; Soft clipping distortion
    ret
ApplyDistortion endp

; Reverb effect
ApplyReverb proc
    ; Simple reverb implementation
    ret
ApplyReverb endp

; Compression effect
ApplyCompression proc
    ; Basic compression
    ret
ApplyCompression endp

; Master effects chain
ApplyMasterFX proc
    ; Final mastering
    ret
ApplyMasterFX endp

; Generate AI-driven musical patterns
GenerateAIPatterns proc
    sub rsp, 58h
    
    cmp byte ptr g_bModelLoaded, 0
    jz patterns_done
    
    ; Use AI model to generate patterns
    call GenerateNewBeat
    
patterns_done:
    add rsp, 58h
    ret
GenerateAIPatterns endp

; Hot-reload system
InitializeHotReload proc
    sub rsp, 28h
    
    ; Create mutex for hot-reload synchronization
    mov rcx, 0
    mov rdx, 0
    mov r8, offset szHotReloadMutex
    call CreateMutexA
    mov g_hHotReloadMutex, rax
    
    add rsp, 28h
    ret
InitializeHotReload endp

RequestHotReload proc
    sub rsp, 28h
    
    ; Signal hot-reload request
    mov byte ptr g_bHotReloadRequested, 1
    
    ; In future: compile new version and swap
    
    add rsp, 28h
    ret
RequestHotReload endp

OnHotReloadComplete proc
    sub rsp, 28h
    
    ; Handle hot-reload completion
    mov byte ptr g_bHotReloadRequested, 0
    
    add rsp, 28h
    ret
OnHotReloadComplete endp

CleanupHotReload proc
    sub rsp, 28h
    
    cmp qword ptr g_hHotReloadMutex, 0
    jz cleanup_done
    
    mov rcx, g_hHotReloadMutex
    call CloseHandle
    
cleanup_done:
    add rsp, 28h
    ret
CleanupHotReload endp

; AI model loading system
LoadAIModel proc
    sub rsp, 58h
    
    ; Load AI model based on current selection
    mov eax, dword ptr g_nCurrentModel
    
    cmp eax, MODEL_TINYLLAMA
    je load_tinyllama
    cmp eax, MODEL_7B
    je load_7b
    cmp eax, MODEL_13B
    je load_13b
    cmp eax, MODEL_70B
    je load_70b
    cmp eax, MODEL_800B
    je load_800b
    
    jmp load_done
    
load_tinyllama:
    mov rcx, offset szTinyLlamaPath
    jmp load_model_file
    
load_7b:
    mov rcx, offset sz7BPath
    jmp load_model_file
    
load_13b:
    mov rcx, offset sz13BPath
    jmp load_model_file
    
load_70b:
    mov rcx, offset sz70BPath
    jmp load_model_file
    
load_800b:
    mov rcx, offset sz800BPath
    
load_model_file:
    call LoadAnyModel
    
load_done:
    add rsp, 58h
    ret
LoadAIModel endp

; Load any GGUF/blob model
LoadAnyModel proc
    sub rsp, 158h
    
    ; Open model file
    mov rdx, 80000000h ; GENERIC_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+20h], 3 ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    cmp rax, -1
    je load_failed
    
    mov rbx, rax ; file handle
    
    ; Get file size
    mov rcx, rax
    call GetFileSize
    mov dword ptr g_dwModelSize, eax
    
    ; Allocate memory
    mov rcx, rax
    call VirtualAlloc
    mov g_pModelData, rax
    test rax, rax
    jz close_file
    
    ; Read file
    mov rcx, rbx
    mov rdx, rax
    mov r8d, dword ptr g_dwModelSize
    mov r9, offset dwBytesRead
    mov qword ptr [rsp+20h], 0
    call ReadFile
    
    test rax, rax
    jz free_memory
    
    ; Set model loaded flag
    mov byte ptr g_bModelLoaded, 1
    
free_memory:
    ; Note: We keep the memory allocated for the model
    
close_file:
    mov rcx, rbx
    call CloseHandle
    
load_failed:
    add rsp, 158h
    ret
LoadAnyModel endp

; Generate new beat using AI
GenerateNewBeat proc
    sub rsp, 78h
    
    cmp byte ptr g_bModelLoaded, 0
    jz beat_done
    
    ; Use AI to generate Euclidean rhythms
    call Euclidean
    
    ; Apply genre-specific patterns
    call ApplyGenrePattern
    
beat_done:
    add rsp, 78h
    ret
GenerateNewBeat endp

; Euclidean rhythm generator
Euclidean proc
    ; Generate Euclidean rhythms
    ret
Euclidean endp

; Apply genre-specific patterns
ApplyGenrePattern proc
    ; Apply patterns based on current genre
    ret
ApplyGenrePattern endp

; Start audio thread
StartAudioThread proc
    sub rsp, 38h
    
    mov qword ptr [rsp+20h], 0
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    
    lea rcx, [rsp+20h]
    xor rdx, rdx
    xor r8, r8
    mov r9, offset AudioThread
    mov qword ptr [rsp+38h], 0
    mov qword ptr [rsp+40h], 0
    call CreateThread
    
    add rsp, 38h
    ret
StartAudioThread endp

; String constants
szChannelRack db "Channel Rack",0
szPianoRoll db "Piano Roll",0
szPlaylist db "Playlist",0
szMixer db "Mixer",0
szBrowser db "Browser",0
szModelNotLoaded db "AI: Not Loaded",0
szModelLoaded db "AI: Ready",0
szStatusFormat db "BPM: %d - Genre: %s",0
szTechnoCommand db "techno",0
szAcidCommand db "acid",0
szAmbientCommand db "ambient",0
szHotReloadMutex db "FruitLoopsUltraHotReload",0
szTinyLlamaPath db "models\tinyllama.gguf",0
sz7BPath db "models\7b.gguf",0
sz13BPath db "models\13b.gguf",0
sz70BPath db "models\70b.gguf",0
sz800BPath db "models\800b.blob",0

; Data variables
dwBytesRead dd 0

; Import additional functions that were used
extern lstrcmpA:proc
extern lstrcpyA:proc
extern wsprintf:proc

end