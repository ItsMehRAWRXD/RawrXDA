IFNDEF USER32_INC
USER32_INC EQU 1

extern MessageBoxW : proc
extern MessageBoxA : proc
extern CreateWindowExW : proc
extern CreateWindowExA : proc
extern ShowWindow : proc
extern UpdateWindow : proc
extern EnableWindow : proc
extern PeekMessageW : proc
extern GetMessageW : proc
extern TranslateMessage : proc
extern DispatchMessageW : proc
extern PostQuitMessage : proc
extern DefWindowProcW : proc
extern RegisterClassExW : proc
extern GetDlgItem : proc
extern SetDlgItemTextW : proc
extern GetDlgItemTextW : proc
extern SetDlgItemInt : proc
extern GetDlgItemInt : proc
extern CheckDlgButton : proc
extern IsDlgButtonChecked : proc
extern SendMessageW : proc
extern SendMessageA : proc
extern GetClientRect : proc
extern GetWindowRect : proc
extern SetWindowPos : proc
extern MoveWindow : proc
extern GetParent : proc
extern GetWindowLongPtrW : proc
extern SetWindowLongPtrW : proc
extern LoadCursorW : proc
extern LoadIconW : proc
extern GetDC : proc
extern ReleaseDC : proc
extern InvalidateRect : proc
extern BeginPaint : proc
extern EndPaint : proc
extern FillRect : proc
extern DrawTextW : proc
extern malloc : proc
extern free : proc
extern strlen : proc
extern memcpy : proc
extern memset : proc

ENDIF
