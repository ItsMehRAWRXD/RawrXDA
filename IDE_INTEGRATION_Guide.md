# RawrXD Text Editor - IDE Integration Guide (C/C++ Wrapper)

## Overview

The RawrXD Text Editor assembly code needs a C/C++ wrapper layer for IDE integration. This guide shows how to create that bridge.

## C++ Wrapper Interface

Create `RawrXD_TextEditor.h`:

```cpp
#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

// ============================================================================
// Structure Definitions (Must match x64 MASM layouts exactly)
// ============================================================================

struct TextBuffer {
    char text_data[2000];       // Offset 0
    uint64_t capacity;          // Offset 2000
    uint64_t used_length;       // Offset 2008
    uint32_t line_count;        // Offset 2016
    uint8_t line_offsets[56];   // Offset 2024
    // Total: 2080 bytes
};

struct Cursor {
    uint64_t byte_offset;       // Offset 0
    uint32_t line_number;       // Offset 8
    uint32_t column_number;     // Offset 12
    uint64_t selection_start;   // Offset 20
    uint64_t selection_end;     // Offset 28
    uint64_t blink_counter;     // Offset 36
    uint8_t reserved[60];       // Offset 44
    // Total: 96 bytes
};

struct EditorWindow {
    HWND hwnd;                  // Offset 0
    HDC hdc;                    // Offset 8
    HFONT hfont;                // Offset 16
    Cursor* cursor_ptr;         // Offset 24
    TextBuffer* buffer_ptr;     // Offset 32
    uint32_t char_width;        // Offset 40
    uint32_t char_height;       // Offset 44
    uint32_t client_width;      // Offset 48
    uint32_t client_height;     // Offset 52
    uint32_t line_num_width;    // Offset 56
    uint32_t scroll_offset_x;   // Offset 60
    uint32_t scroll_offset_y;   // Offset 64
    HBITMAP hbitmap;            // Offset 68
    HDC hmemdc;                 // Offset 76
    uint32_t timer_id;          // Offset 84
    uint8_t reserved[12];       // Offset 88
    // Total: 96 bytes
};

// ============================================================================
// Assembly Function Declarations (Extern "C" for mangling avoidance)
// ============================================================================

extern "C" {
    // Window Management
    void EditorWindow_RegisterClass(void);
    HWND EditorWindow_Create(EditorWindow* window_ptr, const wchar_t* title);
    LRESULT CALLBACK EditorWindow_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    
    // Rendering
    void EditorWindow_ClearBackground(EditorWindow* window_ptr);
    void EditorWindow_RenderLineNumbers(EditorWindow* window_ptr);
    void EditorWindow_RenderText(EditorWindow* window_ptr);
    void EditorWindow_RenderSelection(EditorWindow* window_ptr);
    void EditorWindow_RenderCursor(EditorWindow* window_ptr);
    void EditorWindow_HandlePaint(EditorWindow* window_ptr);
    
    // Input
    void EditorWindow_HandleKeyDown(EditorWindow* window_ptr, uint64_t vk_code);
    void EditorWindow_HandleChar(EditorWindow* window_ptr, uint64_t char_code);
    void EditorWindow_HandleMouseClick(EditorWindow* window_ptr, uint64_t x_pos, uint64_t y_pos);
    
    // File I/O
    const char* EditorWindow_FileOpen(EditorWindow* window_ptr);
    uint32_t EditorWindow_FileSave(EditorWindow* window_ptr, const char* filename);
    void EditorWindow_UpdateStatus(EditorWindow* window_ptr, const char* status_str);
    
    // Menus
    HMENU EditorWindow_CreateMenuBar(EditorWindow* window_ptr);
    HWND EditorWindow_CreateToolbar(EditorWindow* window_ptr);
    HWND EditorWindow_CreateStatusBar(EditorWindow* window_ptr);
    
    // Cursor Navigation
    void Cursor_MoveLeft(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_MoveRight(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_MoveUp(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_MoveDown(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_MoveHome(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_MoveEnd(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_PageUp(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    void Cursor_PageDown(Cursor* cursor_ptr, TextBuffer* buffer_ptr);
    bool Cursor_GetBlink(Cursor* cursor_ptr);
    
    // Text Buffer
    void TextBuffer_InsertChar(TextBuffer* buffer_ptr, uint64_t position, uint8_t char_value);
    void TextBuffer_DeleteChar(TextBuffer* buffer_ptr, uint64_t position);
    const char* TextBuffer_IntToAscii(uint64_t integer_value, char* buffer_ptr, uint64_t radix);
    
    // AI Completion
    void Completion_InsertToken(TextBuffer* buffer_ptr, uint8_t token_byte, Cursor* cursor_ptr);
    void Completion_InsertTokenString(TextBuffer* buffer_ptr, const char* token_string, Cursor* cursor_ptr);
    void Completion_AcceptSelection(TextBuffer* buffer_ptr, uint64_t start_pos, uint64_t end_pos);
    void Completion_Stream(TextBuffer* buffer_ptr, const uint8_t* token_array, uint64_t token_count, Cursor* cursor_ptr);
    
    // Clipboard
    void EditorWindow_Cut(EditorWindow* window_ptr);
    void EditorWindow_Copy(EditorWindow* window_ptr);
    void EditorWindow_Paste(EditorWindow* window_ptr);
}

// ============================================================================
// C++ Wrapper Class (Convenience Layer)
// ============================================================================

class RawrXDTextEditor {
private:
    EditorWindow window_struct;
    Cursor cursor_struct;
    TextBuffer buffer_struct;
    
public:
    RawrXDTextEditor() {
        ZeroMemory(&window_struct, sizeof(EditorWindow));
        ZeroMemory(&cursor_struct, sizeof(Cursor));
        ZeroMemory(&buffer_struct, sizeof(TextBuffer));
        
        // Link structures
        window_struct.cursor_ptr = &cursor_struct;
        window_struct.buffer_ptr = &buffer_struct;
    }
    
    ~RawrXDTextEditor() {
        if (window_struct.hwnd) {
            DestroyWindow(window_struct.hwnd);
        }
        if (window_struct.hdc) {
            ReleaseDC(window_struct.hwnd, window_struct.hdc);
        }
    }
    
    // ---- Window Management ----
    HWND Create(const wchar_t* title = L"RawrXD Text Editor") {
        EditorWindow_RegisterClass();
        return EditorWindow_Create(&window_struct, title);
    }
    
    HWND GetWindowHandle() const {
        return window_struct.hwnd;
    }
    
    int GetWidth() const {
        return window_struct.client_width;
    }
    
    int GetHeight() const {
        return window_struct.client_height;
    }
    
    // ---- Text Manipulation ----
    bool LoadFile(const char* filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 
                                   NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD file_size = GetFileSize(hFile, NULL);
        if (file_size > 2000 - 1) {
            file_size = 2000 - 1;
        }
        
        DWORD bytes_read;
        ReadFile(hFile, buffer_struct.text_data, file_size, &bytes_read, NULL);
        CloseHandle(hFile);
        
        buffer_struct.used_length = bytes_read;
        buffer_struct.text_data[bytes_read] = '\0';
        
        cursor_struct.byte_offset = 0;
        cursor_struct.line_number = 0;
        cursor_struct.column_number = 0;
        
        InvalidateRect(window_struct.hwnd, NULL, FALSE);
        return true;
    }
    
    bool SaveFile(const char* filename) {
        HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0,
                                   NULL, CREATE_ALWAYS, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD bytes_written;
        WriteFile(hFile, buffer_struct.text_data, buffer_struct.used_length, 
                  &bytes_written, NULL);
        CloseHandle(hFile);
        
        return bytes_written == buffer_struct.used_length;
    }
    
    std::string GetText() const {
        return std::string(buffer_struct.text_data, buffer_struct.used_length);
    }
    
    void SetText(const std::string& text) {
        size_t size = text.length();
        if (size > 1999) size = 1999;
        
        memcpy(buffer_struct.text_data, text.c_str(), size);
        buffer_struct.text_data[size] = '\0';
        buffer_struct.used_length = size;
        
        InvalidateRect(window_struct.hwnd, NULL, FALSE);
    }
    
    // ---- Cursor Operations ----
    uint64_t GetCursorPosition() const {
        return cursor_struct.byte_offset;
    }
    
    void SetCursorPosition(uint64_t offset) {
        if (offset > buffer_struct.used_length) {
            offset = buffer_struct.used_length;
        }
        cursor_struct.byte_offset = offset;
    }
    
    int GetCursorLine() const {
        return cursor_struct.line_number;
    }
    
    int GetCursorColumn() const {
        return cursor_struct.column_number;
    }
    
    // ---- AI Token Streaming ----
    void InsertToken(char token) {
        Completion_InsertToken(&buffer_struct, static_cast<uint8_t>(token), &cursor_struct);
        InvalidateRect(window_struct.hwnd, NULL, FALSE);
    }
    
    void InsertTokens(const std::string& tokens) {
        Completion_InsertTokenString(&buffer_struct, tokens.c_str(), &cursor_struct);
        InvalidateRect(window_struct.hwnd, NULL, FALSE);
    }
    
    void StreamTokens(const uint8_t* tokens, size_t count) {
        Completion_Stream(&buffer_struct, tokens, count, &cursor_struct);
        InvalidateRect(window_struct.hwnd, NULL, FALSE);
    }
    
    // ---- Clipboard Operations ----
    void Cut() {
        EditorWindow_Cut(&window_struct);
    }
    
    void Copy() {
        EditorWindow_Copy(&window_struct);
    }
    
    void Paste() {
        EditorWindow_Paste(&window_struct);
    }
    
    // ---- Status Bar ----
    void SetStatus(const std::string& status) {
        EditorWindow_UpdateStatus(&window_struct, status.c_str());
    }
    
    void SetStatusCursorPos() {
        char buf[64];
        snprintf(buf, sizeof(buf), "Line %d, Col %d", 
                cursor_struct.line_number + 1,
                cursor_struct.column_number + 1);
        SetStatus(buf);
    }
    
    // ---- Rendering ----
    void Repaint() {
        InvalidateRect(window_struct.hwnd, NULL, FALSE);
    }
};
```

## Integration with IDE Frame

Create `IDE_Integration.cpp`:

```cpp
#include "RawrXD_TextEditor.h"
#include <windows.h>

class IDEFrame {
private:
    HWND hWnd;
    RawrXDTextEditor* pEditor;
    
public:
    IDEFrame() : hWnd(NULL), pEditor(NULL) {}
    
    ~IDEFrame() {
        if (pEditor) {
            delete pEditor;
        }
    }
    
    HWND CreateFrame() {
        // Create main IDE window
        HWND hFrame = CreateWindow(L"MAINFRAME", L"RawrXD IDE",
                                   WS_OVERLAPPEDWINDOW,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   1200, 800,
                                   NULL, NULL, GetModuleHandle(NULL), this);
        
        hWnd = hFrame;
        
        // Create embedded text editor
        pEditor = new RawrXDTextEditor();
        HWND hEditor = pEditor->Create(L"");
        
        // Parent editor to frame
        SetParent(hEditor, hFrame);
        SetWindowPos(hEditor, HWND_TOP, 10, 40, 1180, 750, SWP_SHOWWINDOW);
        
        return hFrame;
    }
    
    // ---- Menu Handler ----
    void OnFileOpen() {
        OPENFILENAMEA ofn = {0};
        char szFile[260] = {0};
        
        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        
        if (GetOpenFileNameA(&ofn)) {
            if (pEditor->LoadFile(szFile)) {
                SetWindowTextA(hWnd, szFile);
                pEditor->SetStatus("File loaded");
            } else {
                MessageBoxA(hWnd, "Failed to open file", "Error", MB_ICONERROR);
            }
        }
    }
    
    void OnFileSave() {
        OPENFILENAMEA ofn = {0};
        char szFile[260] = "untitled.txt";
        
        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        
        if (GetSaveFileNameA(&ofn)) {
            if (pEditor->SaveFile(szFile)) {
                pEditor->SetStatus("File saved");
            } else {
                MessageBoxA(hWnd, "Failed to save file", "Error", MB_ICONERROR);
            }
        }
    }
    
    void OnAICompletion(const std::string& tokens) {
        // Called when AI model generates tokens
        pEditor->InsertTokens(tokens);
        pEditor->SetStatusCursorPos();
    }
    
    // ---- Message Handler ----
    static LRESULT CALLBACK WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        IDEFrame* pThis = NULL;
        
        if (msg == WM_CREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = static_cast<IDEFrame*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = reinterpret_cast<IDEFrame*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        }
        
        if (!pThis) {
            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        
        switch (msg) {
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case 1001: pThis->OnFileOpen(); break;     // File > Open
                    case 1002: pThis->OnFileSave(); break;     // File > Save
                    case 1003: PostQuitMessage(0); break;      // Exit
                }
                break;
            
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            
            default:
                return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        
        return 0;
    }
};

// ---- Main Application Entry Point ----
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
    
    // Register main frame window class
    WNDCLASS wnd = {0};
    wnd.lpfnWndProc = IDEFrame::WinProc;
    wnd.lpszClassName = L"MAINFRAME";
    wnd.hInstance = hInstance;
    wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd.hbrBackground = (HBRUSH)COLOR_WINDOW + 1;
    RegisterClass(&wnd);
    
    // Create IDE frame
    IDEFrame ide;
    HWND hFrame = ide.CreateFrame();
    ShowWindow(hFrame, nCmdShow);
    UpdateWindow(hFrame);
    
    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return msg.wParam;
}
```

## AI Integration Example

Create `AI_Stream_Handler.cpp`:

```cpp
#include "RawrXD_TextEditor.h"
#include <thread>
#include <queue>

class AICompletionHandler {
private:
    RawrXDTextEditor* pEditor;
    std::queue<std::string> tokenQueue;
    std::thread workerThread;
    bool running;
    
public:
    AICompletionHandler(RawrXDTextEditor* editor) 
        : pEditor(editor), running(false) {}
    
    ~AICompletionHandler() {
        Stop();
    }
    
    void Start() {
        running = true;
        workerThread = std::thread(&AICompletionHandler::ProcessLoop, this);
    }
    
    void Stop() {
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
    
    // Called from AI inference thread
    void OnTokenGenerated(const std::string& token) {
        // Thread-safe queue insert
        tokenQueue.push(token);
    }
    
private:
    void ProcessLoop() {
        while (running) {
            if (!tokenQueue.empty()) {
                std::string token = tokenQueue.front();
                tokenQueue.pop();
                
                // Insert into editor on main thread
                pEditor->InsertTokens(token);
                pEditor->SetStatusCursorPos();
                pEditor->Repaint();
            }
            
            Sleep(10);  // Yield briefly
        }
    }
};

// Example: Stream tokens from LLaMA model
void StreamLlamaCompletion(RawrXDTextEditor* pEditor) {
    AICompletionHandler handler(pEditor);
    handler.Start();
    
    // Simulate model inference generating tokens
    const char* tokens[] = { "T", "h", "e", " ", "q", "u", "i", "c", "k", " ", 
                             "b", "r", "o", "w", "n", " ", "f", "o", "x" };
    
    for (const char* token : tokens) {
        handler.OnTokenGenerated(token);
        Sleep(50);  // 50ms per token (simulating model latency)
    }
    
    Sleep(500);  // Wait for processing
    handler.Stop();
}
```

## Platform-Specific Considerations

### 64-bit vs 32-bit

The current implementation targets x64 (64-bit). For 32-bit:

1. Use ml.exe instead of ml64.exe
2. Adjust structure sizes and offsets
3. Use DWORD instead of QWORD

```asm
; 32-bit version
EditorWindow_Create PROC NEAR
    mov ecx, [esp + 4]       ; First param (32-bit)
    mov edx, [esp + 8]       ; Second param
    ...
EditorWindow_Create ENDP
```

### Windows XP Compatibility

If targeting Windows XP:

1. Don't use modern API functions:
   - ✗ SetWindowSubclass
   - ✗ GetOpenFileNameU
   - ✓ GetOpenFileNameA

2. Link against libraries with XP support:
```cmd
link ... /subsystem:windows,5.1 ...
```

### Console Application

To create console version for debugging:

```cmd
link /subsystem:console /entry:mainCRTStartup ...
```

Then remove GUI code, keep text buffer operations.

## Build System Integration

### CMake Example

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(RawrXDEditor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MASM setup
enable_language(ASM_MASM)

# Source files
set(SOURCES
    RawrXD_TextEditorGUI.asm
    RawrXD_TextEditor_Main.asm
    RawrXD_TextEditor_Completion.asm
    IDE_Integration.cpp
    AI_Stream_Handler.cpp
)

# Create executable
add_executable(RawrXDEditor ${SOURCES})

# Link libraries
target_link_libraries(RawrXDEditor PRIVATE
    kernel32
    user32
    gdi32
)

# Set subsystem
if(MSVC)
    set_target_properties(RawrXDEditor PROPERTIES
        WIN32_EXECUTABLE ON
        LINK_FLAGS "/subsystem:windows /entry:wWinMain"
    )
endif()
```

## Virtual Studio Solution

Create Visual Studio solution structure:

```
RawrXDEditor.sln
├── RawrXDEditor (project)
│   ├── RawrXD_TextEditorGUI.asm
│   ├── RawrXD_TextEditor_Main.asm
│   ├── RawrXD_TextEditor_Completion.asm
│   ├── RawrXD_TextEditor.h
│   ├── IDE_Integration.cpp
│   ├── AI_Stream_Handler.cpp
│   └── RawrXDEditor.vcxproj
```

In .vcxproj:

```xml
<ItemGroup>
    <MASM Include="RawrXD_TextEditorGUI.asm" />
    <MASM Include="RawrXD_TextEditor_Main.asm" />
    <MASM Include="RawrXD_TextEditor_Completion.asm" />
</ItemGroup>

<ItemGroup>
    <ClCompile Include="IDE_Integration.cpp" />
    <ClCompile Include="AI_Stream_Handler.cpp" />
</ItemGroup>

<ItemGroup>
    <ClInclude Include="RawrXD_TextEditor.h" />
</ItemGroup>
```

## Thread Safety

For multi-threaded AI token streaming:

```cpp
class ThreadSafeEditor {
private:
    RawrXDTextEditor editor;
    CRITICAL_SECTION cs;
    
public:
    ThreadSafeEditor() {
        InitializeCriticalSection(&cs);
    }
    
    ~ThreadSafeEditor() {
        DeleteCriticalSection(&cs);
    }
    
    void InsertTokenThreadSafe(char token) {
        EnterCriticalSection(&cs);
        editor.InsertToken(token);
        LeaveCriticalSection(&cs);
    }
};
```

## Deployment Considerations

1. **DLL Export**: Wrap assembly functions in DLL for distribution
2. **Version Strings**: Embed version info in executable
3. **Manifest**: Include application manifest for Windows 7+ compatibility
4. **Code Signing**: Sign executable for Windows SmartScreen
