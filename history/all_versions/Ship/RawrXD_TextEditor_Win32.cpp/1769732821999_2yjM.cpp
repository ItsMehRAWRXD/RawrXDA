/*
 * RawrXD_TextEditor_Win32.cpp
 * Pure Win32 replacement for Qt QPlainTextEdit/QTextEdit
 * Uses: Win32 Edit Control, GDI for rendering
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>

struct LineInfo {
    std::string text;
    size_t lineNumber;
};

class RawrXDTextEditor {
private:
    HWND m_hwnd;
    std::vector<LineInfo> m_lines;
    size_t m_currentLine;
    size_t m_currentCol;
    mutable CRITICAL_SECTION m_criticalSection;
    
public:
    RawrXDTextEditor() : m_hwnd(nullptr), m_currentLine(0), m_currentCol(0) {
        InitializeCriticalSection(&m_criticalSection);
        m_lines.push_back({std::string(), 0});
    }
    
    ~RawrXDTextEditor() {
        DeleteCriticalSection(&m_criticalSection);
    }
    
    void SetWindowHandle(HWND hwnd) {
        m_hwnd = hwnd;
    }
    
    void InsertText(const char* text, size_t length) {
        EnterCriticalSection(&m_criticalSection);
        
        if (m_currentLine >= m_lines.size()) {
            m_currentLine = m_lines.size() - 1;
        }
        
        LineInfo& line = m_lines[m_currentLine];
        line.text.insert(m_currentCol, text, length);
        m_currentCol += length;
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void DeleteChar(size_t count = 1) {
        EnterCriticalSection(&m_criticalSection);
        
        if (m_currentLine < m_lines.size()) {
            LineInfo& line = m_lines[m_currentLine];
            if (m_currentCol > 0) {
                size_t delCount = std::min(count, m_currentCol);
                line.text.erase(m_currentCol - delCount, delCount);
                m_currentCol -= delCount;
            }
        }
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void NewLine() {
        EnterCriticalSection(&m_criticalSection);
        
        if (m_currentLine < m_lines.size()) {
            LineInfo& currentLine = m_lines[m_currentLine];
            std::string remainder = currentLine.text.substr(m_currentCol);
            currentLine.text = currentLine.text.substr(0, m_currentCol);
            
            LineInfo newLine;
            newLine.text = remainder;
            newLine.lineNumber = m_currentLine + 1;
            
            m_lines.insert(m_lines.begin() + m_currentLine + 1, newLine);
            m_currentLine++;
            m_currentCol = 0;
        }
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void SetText(const char* text) {
        EnterCriticalSection(&m_criticalSection);
        
        m_lines.clear();
        std::string content(text);
        
        size_t lineNum = 0;
        size_t pos = 0;
        size_t lastPos = 0;
        
        while (pos < content.length()) {
            if (content[pos] == '\n') {
                LineInfo line;
                line.text = content.substr(lastPos, pos - lastPos);
                line.lineNumber = lineNum;
                m_lines.push_back(line);
                lastPos = pos + 1;
                lineNum++;
            }
            pos++;
        }
        
        if (lastPos < content.length()) {
            LineInfo line;
            line.text = content.substr(lastPos);
            line.lineNumber = lineNum;
            m_lines.push_back(line);
        }
        
        if (m_lines.empty()) {
            m_lines.push_back({std::string(), 0});
        }
        
        m_currentLine = 0;
        m_currentCol = 0;
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void GetText(char* buffer, size_t bufSize) const {
        EnterCriticalSection(&m_criticalSection);
        
        std::string result;
        for (const auto& line : m_lines) {
            result += line.text + "\n";
        }
        
        size_t copyLen = std::min(bufSize - 1, result.size());
        strncpy_s(buffer, bufSize, result.c_str(), copyLen);
        buffer[copyLen] = '\0';
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    size_t GetLineCount() const {
        EnterCriticalSection(&m_criticalSection);
        size_t count = m_lines.size();
        LeaveCriticalSection(&m_criticalSection);
        return count;
    }
    
    bool GetLine(size_t lineNum, char* buffer, size_t bufSize) const {
        EnterCriticalSection(&m_criticalSection);
        
        if (lineNum >= m_lines.size()) {
            LeaveCriticalSection(&m_criticalSection);
            return false;
        }
        
        const LineInfo& line = m_lines[lineNum];
        size_t copyLen = std::min(bufSize - 1, line.text.size());
        strncpy_s(buffer, bufSize, line.text.c_str(), copyLen);
        buffer[copyLen] = '\0';
        
        LeaveCriticalSection(&m_criticalSection);
        return true;
    }
    
    void GetCursorPosition(size_t& line, size_t& col) const {
        EnterCriticalSection(&m_criticalSection);
        line = m_currentLine;
        col = m_currentCol;
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void SetCursorPosition(size_t line, size_t col) {
        EnterCriticalSection(&m_criticalSection);
        
        if (line < m_lines.size()) {
            m_currentLine = line;
            m_currentCol = std::min(col, m_lines[line].text.length());
        }
        
        LeaveCriticalSection(&m_criticalSection);
    }
    
    void Clear() {
        EnterCriticalSection(&m_criticalSection);
        
        m_lines.clear();
        m_lines.push_back({std::string(), 0});
        m_currentLine = 0;
        m_currentCol = 0;
        
        LeaveCriticalSection(&m_criticalSection);
    }
};

static RawrXDTextEditor* g_textEditor = nullptr;

extern "C" {
    __declspec(dllexport) void* __stdcall CreateTextEditor() {
        if (!g_textEditor) {
            g_textEditor = new RawrXDTextEditor();
        }
        return g_textEditor;
    }
    
    __declspec(dllexport) void __stdcall DestroyTextEditor(void* editor) {
        if (editor && editor == g_textEditor) {
            delete g_textEditor;
            g_textEditor = nullptr;
        }
    }
    
    __declspec(dllexport) void __stdcall TextEditor_InsertText(void* editor, const char* text, size_t length) {
        RawrXDTextEditor* e = static_cast<RawrXDTextEditor*>(editor);
        if (e) e->InsertText(text, length);
    }
    
    __declspec(dllexport) void __stdcall TextEditor_DeleteChar(void* editor, size_t count) {
        RawrXDTextEditor* e = static_cast<RawrXDTextEditor*>(editor);
        if (e) e->DeleteChar(count);
    }
    
    __declspec(dllexport) void __stdcall TextEditor_NewLine(void* editor) {
        RawrXDTextEditor* e = static_cast<RawrXDTextEditor*>(editor);
        if (e) e->NewLine();
    }
    
    __declspec(dllexport) void __stdcall TextEditor_SetText(void* editor, const char* text) {
        RawrXDTextEditor* e = static_cast<RawrXDTextEditor*>(editor);
        if (e) e->SetText(text);
    }
    
    __declspec(dllexport) void __stdcall TextEditor_GetText(void* editor, char* buffer, size_t bufSize) {
        RawrXDTextEditor* e = static_cast<RawrXDTextEditor*>(editor);
        if (e) e->GetText(buffer, bufSize);
    }
    
    __declspec(dllexport) size_t __stdcall TextEditor_GetLineCount(void* editor) {
        RawrXDTextEditor* e = static_cast<RawrXDTextEditor*>(editor);
        return e ? e->GetLineCount() : 0;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugStringW(L"RawrXD_TextEditor_Win32 loaded\n");
    } else if (fdwReason == DLL_PROCESS_DETACH && g_textEditor) {
        delete g_textEditor;
    }
    return TRUE;
}
