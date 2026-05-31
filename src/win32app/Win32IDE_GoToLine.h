#pragma once
#include <windows.h>
#include <cstdint>

/**
 * @brief Show a "Go to Line" modal dialog.
 * @param hParent Parent window handle.
 * @return void
 *
 * Production implementation: creates a small modal dialog with an edit box
 * for line number input and OK/Cancel buttons. On OK, posts a WM_GOTO_LINE
 * message to the parent with the entered line number.
 */
void ShowGoToLineDialog(HWND hParent);
