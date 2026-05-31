/*=============================================================================
 * minimonaco_bridge.h
 * C API bridge between RawrXD Win32IDE (C) and MiniMonaco Editor (C++)
 *
 * This header is pure C and can be included from RawrXD_IDE_Win32.cpp.
 * The implementation (minimonaco_bridge.cpp) is C++ and links against
 * MiniMonaco.lib.
 *===========================================================================*/

#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to a MiniMonaco editor instance */
typedef struct MiniMonacoEditor MiniMonacoEditor;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */
MiniMonacoEditor* minimonaco_create(HWND parent, int x, int y, int width, int height);
void              minimonaco_destroy(MiniMonacoEditor* ed);

/* ── Text operations ─────────────────────────────────────────────────────── */
void minimonaco_set_text(MiniMonacoEditor* ed, const wchar_t* text);
void minimonaco_get_text(MiniMonacoEditor* ed, wchar_t* out, size_t outLen);
void minimonaco_clear(MiniMonacoEditor* ed);

/* ── Edit commands ──────────────────────────────────────────────────────── */
void minimonaco_undo(MiniMonacoEditor* ed);
void minimonaco_redo(MiniMonacoEditor* ed);
void minimonaco_cut(MiniMonacoEditor* ed);
void minimonaco_copy(MiniMonacoEditor* ed);
void minimonaco_paste(MiniMonacoEditor* ed);
void minimonaco_select_all(MiniMonacoEditor* ed);
void minimonaco_delete(MiniMonacoEditor* ed);

/* ── Insert ──────────────────────────────────────────────────────────────── */
void minimonaco_insert_text(MiniMonacoEditor* ed, const wchar_t* text);
void minimonaco_insert_newline(MiniMonacoEditor* ed);
void minimonaco_insert_tab(MiniMonacoEditor* ed);

/* ── Navigation ──────────────────────────────────────────────────────────── */
void minimonaco_goto_line(MiniMonacoEditor* ed, int line);
void minimonaco_goto_pos(MiniMonacoEditor* ed, size_t pos);
int  minimonaco_current_line(MiniMonacoEditor* ed);
int  minimonaco_current_column(MiniMonacoEditor* ed);
int  minimonaco_line_count(MiniMonacoEditor* ed);

/* ── Find / Replace ─────────────────────────────────────────────────────── */
void minimonaco_find(MiniMonacoEditor* ed, const wchar_t* pattern, int caseSensitive);
void minimonaco_find_next(MiniMonacoEditor* ed);
void minimonaco_find_prev(MiniMonacoEditor* ed);
void minimonaco_replace(MiniMonacoEditor* ed, const wchar_t* replacement);
void minimonaco_replace_all(MiniMonacoEditor* ed, const wchar_t* pattern, const wchar_t* replacement);

/* ── Selection ──────────────────────────────────────────────────────────── */
BOOL minimonaco_has_selection(MiniMonacoEditor* ed);
void minimonaco_get_selection(MiniMonacoEditor* ed, wchar_t* out, size_t outLen);

/* ── Theme ──────────────────────────────────────────────────────────────── */
void minimonaco_set_dark_theme(MiniMonacoEditor* ed);
void minimonaco_set_light_theme(MiniMonacoEditor* ed);
void minimonaco_set_font(MiniMonacoEditor* ed, const wchar_t* face, int size);

/* ── Windowing ─────────────────────────────────────────────────────────── */
void minimonaco_show(MiniMonacoEditor* ed);
void minimonaco_hide(MiniMonacoEditor* ed);
void minimonaco_move(MiniMonacoEditor* ed, int x, int y, int width, int height);
void minimonaco_set_focus(MiniMonacoEditor* ed);
BOOL minimonaco_is_visible(MiniMonacoEditor* ed);
HWND minimonaco_hwnd(MiniMonacoEditor* ed);

/* ── File I/O helpers ──────────────────────────────────────────────────── */
BOOL minimonaco_load_file(MiniMonacoEditor* ed, const wchar_t* path);
BOOL minimonaco_save_file(MiniMonacoEditor* ed, const wchar_t* path);

/* ── Modification tracking ──────────────────────────────────────────────── */
BOOL minimonaco_is_modified(MiniMonacoEditor* ed);
void minimonaco_set_modified(MiniMonacoEditor* ed, BOOL modified);

/* ── Language ──────────────────────────────────────────────────────────── */
void minimonaco_set_language(MiniMonacoEditor* ed, const char* lang); /* "cpp", "python", "text" */

#ifdef __cplusplus
}
#endif
