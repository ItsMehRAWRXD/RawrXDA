/*=============================================================================
 * minimonaco_bridge.cpp
 * C++ implementation of the C bridge for RawrXD Win32IDE
 *===========================================================================*/

#include "minimonaco_bridge.h"
#include "minimonaco.h"

#include <string>
#include <fstream>
#include <vector>

/* ── Internal wrapper ────────────────────────────────────────────────────── */
struct MiniMonacoEditor {
    MiniMonaco::Editor* editor = nullptr;
    HWND                hwnd = nullptr;
    BOOL                modified = FALSE;
    std::wstring        currentFile;
};

/* ── Lifecycle ────────────────────────────────────────────────────────────── */
extern "C" MiniMonacoEditor* minimonaco_create(HWND parent, int x, int y, int width, int height) {
    if (!parent) return nullptr;

    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    if (!hInstance) hInstance = GetModuleHandleW(nullptr);

    if (!MiniMonaco::EditorWindow::Register(hInstance)) {
        /* Already registered is OK */
    }

    MiniMonaco::Config cfg;
    cfg.fontSize = 11;
    cfg.fontFamily = L"Consolas";
    cfg.showLineNumbers = TRUE;
    cfg.showMinimap = TRUE;
    cfg.autoClose = TRUE;
    cfg.tabSize = 4;

    MiniMonaco::Editor* ed = MiniMonaco::EditorWindow::Create(parent, x, y, width, height, cfg);
    if (!ed) return nullptr;

    MiniMonacoEditor* wrapper = new MiniMonacoEditor();
    wrapper->editor = ed;
    wrapper->hwnd = MiniMonaco::EditorWindow::FromHwnd(
        FindWindowExW(parent, nullptr, MiniMonaco::EditorWindow::ClassName(), nullptr)
    ) ? nullptr : nullptr; /* hwnd resolved on demand */

    /* Track modification */
    ed->setChangeCallback([wrapper]() {
        wrapper->modified = TRUE;
    });

    return wrapper;
}

extern "C" void minimonaco_destroy(MiniMonacoEditor* ed) {
    if (!ed) return;
    if (ed->hwnd) {
        DestroyWindow(ed->hwnd);
    }
    delete ed;
}

/* ── Text operations ──────────────────────────────────────────────────────── */
extern "C" void minimonaco_set_text(MiniMonacoEditor* ed, const wchar_t* text) {
    if (!ed || !ed->editor || !text) return;
    ed->editor->setText(text);
    ed->modified = FALSE;
}

extern "C" void minimonaco_get_text(MiniMonacoEditor* ed, wchar_t* out, size_t outLen) {
    if (!ed || !ed->editor || !out || outLen == 0) return;
    std::wstring text = ed->editor->text();
    wcsncpy_s(out, outLen, text.c_str(), _TRUNCATE);
}

extern "C" void minimonaco_clear(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->setText(L"");
    ed->modified = FALSE;
}

/* ── Edit commands ──────────────────────────────────────────────────────── */
extern "C" void minimonaco_undo(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->undo();
}

extern "C" void minimonaco_redo(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->redo();
}

extern "C" void minimonaco_cut(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->cut();
}

extern "C" void minimonaco_copy(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->copy();
}

extern "C" void minimonaco_paste(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->paste();
}

extern "C" void minimonaco_select_all(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->selectAll();
}

extern "C" void minimonaco_delete(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->del();
}

/* ── Insert ──────────────────────────────────────────────────────────────── */
extern "C" void minimonaco_insert_text(MiniMonacoEditor* ed, const wchar_t* text) {
    if (!ed || !ed->editor || !text) return;
    ed->editor->insert(text);
}

extern "C" void minimonaco_insert_newline(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->insertNewline();
}

extern "C" void minimonaco_insert_tab(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->insertTab();
}

/* ── Navigation ──────────────────────────────────────────────────────────── */
extern "C" void minimonaco_goto_line(MiniMonacoEditor* ed, int line) {
    if (!ed || !ed->editor) return;
    ed->editor->goToLine(line);
}

extern "C" void minimonaco_goto_pos(MiniMonacoEditor* ed, size_t pos) {
    if (!ed || !ed->editor) return;
    ed->editor->goToPosition(pos);
}

extern "C" int minimonaco_current_line(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return 0;
    return ed->editor->currentLine();
}

extern "C" int minimonaco_current_column(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return 0;
    return ed->editor->currentColumn();
}

extern "C" int minimonaco_line_count(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return 0;
    return ed->editor->lineCount();
}

/* ── Find / Replace ─────────────────────────────────────────────────────── */
extern "C" void minimonaco_find(MiniMonacoEditor* ed, const wchar_t* pattern, int caseSensitive) {
    if (!ed || !ed->editor || !pattern) return;
    ed->editor->find(pattern, caseSensitive != 0);
}

extern "C" void minimonaco_find_next(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->findNext();
}

extern "C" void minimonaco_find_prev(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    ed->editor->findPrev();
}

extern "C" void minimonaco_replace(MiniMonacoEditor* ed, const wchar_t* replacement) {
    if (!ed || !ed->editor || !replacement) return;
    ed->editor->replace(replacement);
}

extern "C" void minimonaco_replace_all(MiniMonacoEditor* ed, const wchar_t* pattern, const wchar_t* replacement) {
    if (!ed || !ed->editor || !pattern || !replacement) return;
    ed->editor->replaceAll(pattern, replacement);
}

/* ── Selection ──────────────────────────────────────────────────────────── */
extern "C" BOOL minimonaco_has_selection(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return FALSE;
    return ed->editor->hasSelection() ? TRUE : FALSE;
}

extern "C" void minimonaco_get_selection(MiniMonacoEditor* ed, wchar_t* out, size_t outLen) {
    if (!ed || !ed->editor || !out || outLen == 0) return;
    std::wstring sel = ed->editor->selection();
    wcsncpy_s(out, outLen, sel.c_str(), _TRUNCATE);
}

/* ── Theme ──────────────────────────────────────────────────────────────── */
extern "C" void minimonaco_set_dark_theme(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    MiniMonaco::Config cfg = ed->editor->config();
    cfg.bgColor = 0xFF1E1E1E;
    cfg.textColor = 0xFFD4D4D4;
    cfg.selectionBg = 0xFF264F78;
    cfg.cursorColor = 0xFFAEAFAD;
    cfg.lineNumberColor = 0xFF858585;
    cfg.lineNumberBg = 0xFF1E1E1E;
    cfg.currentLineBg = 0xFF2C2C2C;
    cfg.searchHighlightBg = 0xFF613613;
    cfg.syntaxColors["keyword"] = 0xFF569CD6;
    cfg.syntaxColors["type"] = 0xFF4EC9B0;
    cfg.syntaxColors["string"] = 0xFFCE9178;
    cfg.syntaxColors["number"] = 0xFFB5CEA8;
    cfg.syntaxColors["comment"] = 0xFF6A9955;
    cfg.syntaxColors["function"] = 0xFFDCDCAA;
    cfg.syntaxColors["operator"] = 0xFFD4D4D4;
    cfg.syntaxColors["preprocessor"] = 0xFF569CD6;
    ed->editor->setConfig(cfg);
}

extern "C" void minimonaco_set_light_theme(MiniMonacoEditor* ed) {
    if (!ed || !ed->editor) return;
    MiniMonaco::Config cfg = ed->editor->config();
    cfg.bgColor = 0xFFFFFFFF;
    cfg.textColor = 0xFF000000;
    cfg.selectionBg = 0xFFADD6FF;
    cfg.cursorColor = 0xFF000000;
    cfg.lineNumberColor = 0xFF237893;
    cfg.lineNumberBg = 0xFFF5F5F5;
    cfg.currentLineBg = 0xFFF5F5F5;
    cfg.searchHighlightBg = 0xFFA8AC94;
    cfg.syntaxColors["keyword"] = 0xFF0000FF;
    cfg.syntaxColors["type"] = 0xFF267F99;
    cfg.syntaxColors["string"] = 0xFFA31515;
    cfg.syntaxColors["number"] = 0xFF098658;
    cfg.syntaxColors["comment"] = 0xFF008000;
    cfg.syntaxColors["function"] = 0xFF795E26;
    cfg.syntaxColors["operator"] = 0xFF000000;
    cfg.syntaxColors["preprocessor"] = 0xFF0000FF;
    ed->editor->setConfig(cfg);
}

extern "C" void minimonaco_set_font(MiniMonacoEditor* ed, const wchar_t* face, int size) {
    if (!ed || !ed->editor || !face) return;
    MiniMonaco::Config cfg = ed->editor->config();
    cfg.fontFamily = face;
    cfg.fontSize = size;
    ed->editor->setConfig(cfg);
}

/* ── Windowing ─────────────────────────────────────────────────────────── */
extern "C" void minimonaco_show(MiniMonacoEditor* ed) {
    if (!ed || !ed->hwnd) return;
    ShowWindow(ed->hwnd, SW_SHOW);
}

extern "C" void minimonaco_hide(MiniMonacoEditor* ed) {
    if (!ed || !ed->hwnd) return;
    ShowWindow(ed->hwnd, SW_HIDE);
}

extern "C" void minimonaco_move(MiniMonacoEditor* ed, int x, int y, int width, int height) {
    if (!ed || !ed->hwnd) return;
    SetWindowPos(ed->hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

extern "C" void minimonaco_set_focus(MiniMonacoEditor* ed) {
    if (!ed || !ed->hwnd) return;
    SetFocus(ed->hwnd);
}

extern "C" BOOL minimonaco_is_visible(MiniMonacoEditor* ed) {
    if (!ed || !ed->hwnd) return FALSE;
    return IsWindowVisible(ed->hwnd);
}

extern "C" HWND minimonaco_hwnd(MiniMonacoEditor* ed) {
    if (!ed) return nullptr;
    return ed->hwnd;
}

/* ── File I/O helpers ──────────────────────────────────────────────────────── */
extern "C" BOOL minimonaco_load_file(MiniMonacoEditor* ed, const wchar_t* path) {
    if (!ed || !ed->editor || !path) return FALSE;

    std::wifstream file(path, std::ios::binary);
    if (!file) return FALSE;

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::wstring text;
    text.resize(static_cast<size_t>(size) / sizeof(wchar_t));
    file.read(reinterpret_cast<wchar_t*>(text.data()), size);

    ed->editor->setText(text);
    ed->currentFile = path;
    ed->modified = FALSE;
    return TRUE;
}

extern "C" BOOL minimonaco_save_file(MiniMonacoEditor* ed, const wchar_t* path) {
    if (!ed || !ed->editor || !path) return FALSE;

    std::wofstream file(path, std::ios::binary);
    if (!file) return FALSE;

    std::wstring text = ed->editor->text();
    file.write(text.c_str(), static_cast<std::streamsize>(text.size()));

    ed->currentFile = path;
    ed->modified = FALSE;
    return TRUE;
}

/* ── Modification tracking ──────────────────────────────────────────────── */
extern "C" BOOL minimonaco_is_modified(MiniMonacoEditor* ed) {
    if (!ed) return FALSE;
    return ed->modified;
}

extern "C" void minimonaco_set_modified(MiniMonacoEditor* ed, BOOL modified) {
    if (!ed) return;
    ed->modified = modified;
}

/* ── Language ──────────────────────────────────────────────────────────── */
extern "C" void minimonaco_set_language(MiniMonacoEditor* ed, const char* lang) {
    if (!ed || !ed->editor || !lang) return;
    ed->editor->setLanguage(lang);
}
