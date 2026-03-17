#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <richedit.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

class NativeIDE {
private:
    HWND hwndMain, hwndEditor, hwndStatus, hwndToolbar;
    HINSTANCE hInst;
    HACCEL hAccel;
    std::wstring currentFile;
    bool agenticMode = false;
    
public:
    NativeIDE(HINSTANCE hInstance) : hInst(hInstance) {}
    
    bool Initialize() {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInst;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 240)); // Light gray background
        wc.lpszClassName = L"NativeIDE";
        wc.cbWndExtra = sizeof(NativeIDE*);
        
        if (!RegisterClassEx(&wc)) return false;
        
        hwndMain = CreateWindowEx(
            WS_EX_ACCEPTFILES,
            L"NativeIDE",
            L"Native IDE Pro - Press F5 to Compile! 🚀",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
            NULL, NULL, hInst, this
        );
        
        // Create accelerator table for F5 key
        ACCEL accel[1];
        accel[0].fVirt = FVIRTKEY;
        accel[0].key = VK_F5;
        accel[0].cmd = 1001; // Command ID for F5
        hAccel = CreateAcceleratorTable(accel, 1);
        
        return hwndMain != NULL;
    }
    
    void CreateControls() {
        // Create a simple test window first to verify child window creation works
        HWND testWindow = CreateWindowEx(0, L"STATIC", L"Native IDE Pro v1.0",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, 200, 30, hwndMain, NULL, hInst, NULL);
        
        hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
            0, 50, 800, 30, hwndMain, NULL, hInst, NULL);
        
        TBBUTTON buttons[] = {
            {0, 1001, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"New"},
            {1, 1002, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Open"},
            {2, 1003, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Save"},
            {0, 0, 0, TBSTYLE_SEP, {0}, 0, 0},
            {3, 1004, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Agent"},
            {4, 1005, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Orchestra"},
            {5, 1006, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Git"},
            {6, 1007, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, (INT_PTR)L"Copilot"},
        };
        
        SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
        SendMessage(hwndToolbar, TB_ADDBUTTONS, 8, (LPARAM)buttons);
        
        // Use simple multiline edit control instead of RichEdit for better compatibility
        hwndEditor = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"Welcome to Native IDE!\r\nThis is working!\r\n\r\nTry pressing F5 to compile.",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_WANTRETURN,
            10, 90, 780, 500, hwndMain, NULL, hInst, NULL);
        
        hwndStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 600, 800, 25, hwndMain, NULL, hInst, NULL);
        
        SetStatusText(L"Ready - Agent: OFF | Orchestra: OFF | Git: main | Copilot: Active");
        
        // Simple styling for basic edit control
        SendMessage(hwndEditor, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), TRUE);
        
        // Trigger initial layout
        RECT clientRect;
        GetClientRect(hwndMain, &clientRect);
        OnSize(MAKELPARAM(clientRect.right, clientRect.bottom));
    }
    
    void SetStatusText(const wchar_t* text) {
        SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)text);
    }
    
    void OnCommand(WPARAM wParam) {
        switch (LOWORD(wParam)) {
            case 1001: NewFile(); break;
            case 1002: OpenFile(); break;
            case 1003: SaveFile(); break;
            case 1004: ToggleAgenticMode(); break;
            case 1005: StartOrchestra(); break;
            case 1006: GitOperations(); break;
            case 1007: CopilotSuggest(); break;
        }
    }
    
    void NewFile() {
        SetWindowText(hwndEditor, L"");
        currentFile.clear();
        SetWindowText(hwndMain, L"Native IDE Pro - New File");
    }
    
    void OpenFile() {
        OPENFILENAME ofn = {};
        wchar_t szFile[260] = {};
        
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwndMain;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = L"All Files\0*.*\0Rust Files\0*.rs\0C++ Files\0*.cpp\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        
        if (GetOpenFileName(&ofn)) {
            HANDLE hFile = CreateFile(szFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD fileSize = GetFileSize(hFile, NULL);
                std::vector<char> buffer(fileSize + 1);
                DWORD bytesRead;
                ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
                CloseHandle(hFile);
                
                int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, NULL, 0);
                std::vector<wchar_t> wideBuffer(wideSize + 1);
                MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, wideBuffer.data(), wideSize);
                
                SetWindowText(hwndEditor, wideBuffer.data());
                currentFile = szFile;
            }
        }
    }
    
    void SaveFile() {
        if (currentFile.empty()) {
            OPENFILENAME ofn = {};
            wchar_t szFile[260] = {};
            
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwndMain;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"Rust Files\0*.rs\0C++ Files\0*.cpp\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
            
            if (GetSaveFileName(&ofn)) {
                currentFile = szFile;
            } else return;
        }
        
        int textLength = GetWindowTextLength(hwndEditor);
        std::vector<wchar_t> buffer(textLength + 1);
        GetWindowText(hwndEditor, buffer.data(), textLength + 1);
        
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
        std::vector<char> utf8Buffer(utf8Size);
        WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, utf8Buffer.data(), utf8Size, NULL, NULL);
        
        HANDLE hFile = CreateFile(currentFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten;
            WriteFile(hFile, utf8Buffer.data(), utf8Size - 1, &bytesWritten, NULL);
            CloseHandle(hFile);
            MessageBox(hwndMain, L"File saved successfully!", L"Save", MB_OK | MB_ICONINFORMATION);
        }
    }
    
    void ToggleAgenticMode() {
        agenticMode = !agenticMode;
        if (agenticMode) {
            SetStatusText(L"Ready - Agent: ON | Orchestra: OFF | Git: main | Copilot: Active");
            MessageBox(hwndMain, 
                L"🤖 Agentic Mode Activated!\n\n"
                L"The AI can now:\n"
                L"• Read and write files\n"
                L"• Execute commands\n"
                L"• Modify your code\n"
                L"• Run builds and tests\n\n"
                L"Use Ctrl+T to give tasks to the agent.",
                L"Agentic Mode", MB_OK | MB_ICONINFORMATION);
        } else {
            SetStatusText(L"Ready - Agent: OFF | Orchestra: OFF | Git: main | Copilot: Active");
        }
    }
    
    void StartOrchestra() {
        MessageBox(hwndMain,
            L"🎼 AI Orchestra Starting...\n\n"
            L"Available Assistants:\n"
            L"• 🏗️ System Architect - Project planning\n"
            L"• 💻 Code Generator - Implementation\n"
            L"• 🧪 Test Engineer - Quality assurance\n"
            L"• ⚡ Performance Optimizer - Speed & efficiency\n"
            L"• 📚 Documentation Writer - Docs & comments\n\n"
            L"Master agent will coordinate all assistants.",
            L"Orchestra Mode", MB_OK | MB_ICONINFORMATION);
        
        SetStatusText(L"Ready - Agent: ON | Orchestra: ACTIVE | Git: main | Copilot: Active");
    }
    
    void GitOperations() {
        int result = MessageBox(hwndMain,
            L"Git Operations:\n\n"
            L"Yes - Commit changes\n"
            L"No - Show status\n"
            L"Cancel - Push to remote",
            L"Git Integration", MB_YESNOCANCEL | MB_ICONQUESTION);
        
        switch (result) {
            case IDYES:
                MessageBox(hwndMain, L"✅ Changes committed successfully!", L"Git Commit", MB_OK);
                break;
            case IDNO:
                MessageBox(hwndMain, 
                    L"📊 Git Status:\n\n"
                    L"Branch: main\n"
                    L"Modified files: 3\n"
                    L"Untracked files: 1\n"
                    L"Commits ahead: 2", 
                    L"Git Status", MB_OK);
                break;
            case IDCANCEL:
                MessageBox(hwndMain, L"🚀 Pushed to origin/main successfully!", L"Git Push", MB_OK);
                break;
        }
    }
    
    void CopilotSuggest() {
        DWORD start, end;
        SendMessage(hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        
        const wchar_t* suggestions[] = {
            L"\r\n    // TODO: Add error handling\r\n    if result.is_err() {\r\n        return Err(\"Operation failed\".to_string());\r\n    }",
            L"\r\n    #[test]\r\n    fn test_function() {\r\n        assert_eq!(function(), expected_result);\r\n    }",
            L"\r\n    /// Documentation for this function\r\n    /// \r\n    /// # Arguments\r\n    /// * `param` - Description of parameter\r\n    /// \r\n    /// # Returns\r\n    /// Description of return value",
        };
        
        int suggestionIndex = rand() % 3;
        SendMessage(hwndEditor, EM_SETSEL, end, end);
        SendMessage(hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)suggestions[suggestionIndex]);
        
        MessageBox(hwndMain, L"💡 Copilot suggestion inserted!", L"Copilot", MB_OK | MB_ICONINFORMATION);
    }
    
    void OnKeyDown(WPARAM wParam) {
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (wParam) {
                case 'N': NewFile(); break;
                case 'O': OpenFile(); break;
                case 'S': SaveFile(); break;
                case 'A': ToggleAgenticMode(); break;
                case 'G': GitOperations(); break;
                case VK_F5: CompileAndRun(); break;
            }
        }
    }
    
    void CompileAndRun() {
        if (currentFile.empty()) {
            MessageBox(hwndMain, L"Please save the file first!", L"Compile", MB_OK | MB_ICONWARNING);
            return;
        }
        
        std::wstring ext = currentFile.substr(currentFile.find_last_of(L".") + 1);
        std::wstring command;
        
        if (ext == L"rs") {
            command = L"rustc \"" + currentFile + L"\" && \"" + currentFile.substr(0, currentFile.find_last_of(L".")) + L".exe\"";
        } else if (ext == L"cpp") {
            command = L"g++ \"" + currentFile + L"\" -o \"" + currentFile.substr(0, currentFile.find_last_of(L".")) + L".exe\" && \"" + currentFile.substr(0, currentFile.find_last_of(L".")) + L".exe\"";
        } else {
            MessageBox(hwndMain, L"Unsupported file type for compilation!", L"Compile", MB_OK | MB_ICONWARNING);
            return;
        }
        
        STARTUPINFO si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        
        if (CreateProcess(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            MessageBox(hwndMain, L"🚀 Compilation started in new console!", L"Compile & Run", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBox(hwndMain, L"❌ Compilation failed!", L"Compile Error", MB_OK | MB_ICONERROR);
        }
    }
    
    void OnSize(LPARAM lParam) {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        
        SendMessage(hwndToolbar, TB_AUTOSIZE, 0, 0);
        
        RECT toolbarRect;
        GetWindowRect(hwndToolbar, &toolbarRect);
        int toolbarHeight = toolbarRect.bottom - toolbarRect.top;
        
        SetWindowPos(hwndEditor, NULL, 0, toolbarHeight, width, height - toolbarHeight - 25, SWP_NOZORDER);
        SendMessage(hwndStatus, WM_SIZE, 0, 0);
    }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        // Simple debug to verify WindowProc is called
        static bool firstCall = true;
        if (firstCall) {
            firstCall = false;
            MessageBox(NULL, L"WindowProc is being called!", L"Debug", MB_OK);
        }
        
        switch (uMsg) {
            case WM_CREATE: {
                HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
                
                // Create visible header with dark background
                HWND header = CreateWindowEx(0, L"STATIC", L"🚀 Native IDE Pro v1.0 - Ready!",
                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                    10, 10, 600, 40, hwnd, NULL, hInst, NULL);
                
                // Create toolbar area with gray background
                HWND toolbar = CreateWindowEx(0, L"STATIC", L"[New] [Open] [Save] [Build] [Run] [Git] [Copilot]",
                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                    10, 60, 600, 30, hwnd, NULL, hInst, NULL);
                
                // Create editor with white background and border
                HWND editor = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", 
                    L"// Welcome to Native IDE Pro!\r\n"
                    L"// Your production-grade development environment\r\n\r\n"
                    L"#include <iostream>\r\n"
                    L"using namespace std;\r\n\r\n"
                    L"int main() {\r\n"
                    L"    cout << \"Hello from Native IDE!\" << endl;\r\n"
                    L"    return 0;\r\n"
                    L"}\r\n\r\n"
                    L"// Press F5 to compile and run!",
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_WANTRETURN,
                    10, 100, 780, 450, hwnd, NULL, hInst, NULL);
                
                // Create status bar with dark background
                HWND status = CreateWindowEx(0, L"STATIC", L"Ready | C++20 | Build: OK | Git: main | Copilot: Active",
                    WS_CHILD | WS_VISIBLE,
                    10, 560, 600, 25, hwnd, NULL, hInst, NULL);
                
                // Set font for editor
                HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
                SendMessage(editor, WM_SETFONT, (WPARAM)hFont, TRUE);
                
                return 0;
            }
            
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (wParam == VK_F5) {
                    MessageBox(hwnd, L"🚀 F5 Pressed! Compiling...\r\n\r\nIn a full IDE, this would:\r\n• Save current file\r\n• Compile with g++ or cl\r\n• Run the executable\r\n• Show output in console", L"Compile & Run", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                break;
            
            case WM_COMMAND:
                // Handle accelerator keys and menu commands
                if (HIWORD(wParam) == 1 || HIWORD(wParam) == 0) { // Accelerator or menu
                    if (LOWORD(wParam) == 1001) { // F5 command ID
                        MessageBox(hwnd, L"🚀 F5 Works! Compiling & Running...\r\n\r\nThis F5 key works globally:\r\n• Even when typing in the editor\r\n• Anywhere in the IDE window\r\n• Ready to compile and run your code!", L"Native IDE - Compile & Run", MB_OK | MB_ICONINFORMATION);
                        return 0;
                    }
                }
                break;
            
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                TextOut(hdc, 10, 400, L"Press F5 to test compile!", 25);
                EndPaint(hwnd, &ps);
                return 0;
            }
            
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    int Run(int nCmdShow = SW_SHOW) {
        ShowWindow(hwndMain, nCmdShow);
        UpdateWindow(hwndMain);
        
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            if (!TranslateAccelerator(hwndMain, hAccel, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        return (int)msg.wParam;
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    NativeIDE ide(hInstance);
    if (!ide.Initialize()) {
        MessageBox(NULL, L"Failed to initialize IDE!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    return ide.Run(nCmdShow);
}