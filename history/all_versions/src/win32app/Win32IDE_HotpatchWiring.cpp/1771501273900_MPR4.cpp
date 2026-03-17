#include "Win32IDE.h"
#include "shared_context.h"
void Win32IDE::InitializeHotpatchGUI() {
    auto& hp = GlobalContext::Get().GetHotPatcher();
    auto& rbac = GlobalContext::Get().GetRBAC();
    
    m_hotpatchList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, 
        WS_CHILD|WS_VISIBLE|LBS_NOTIFY|WS_VSCROLL, 0, 0, 300, 400, 
        m_hwndHotpatchPanel, (HMENU)9001, GetModuleHandle(nullptr), nullptr);
        
    m_applyPatchBtn = CreateWindowExW(0, L"BUTTON", L"Apply Patch", 
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 310, 10, 100, 30, 
        m_hwndHotpatchPanel, (HMENU)9002, GetModuleHandle(nullptr), nullptr);
        
    m_revertPatchBtn = CreateWindowExW(0, L"BUTTON", L"Revert Patch", 
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_DISABLED, 310, 50, 100, 30, 
        m_hwndHotpatchPanel, (HMENU)9003, GetModuleHandle(nullptr), nullptr);
        
    SetWindowSubclass(m_hwndHotpatchPanel, HotpatchPanelProc, 9000, (DWORD_PTR)this);
}

LRESULT CALLBACK Win32IDE::HotpatchPanelProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uid, DWORD_PTR dwRef) {
    auto* pThis = (Win32IDE*)dwRef;
    if(msg == WM_COMMAND) {
        if(LOWORD(wp) == 9002) {
            wchar_t path[MAX_PATH];
            OPENFILENAMEW ofn = {sizeof(ofn)};
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"Patch Files (*.rawr)\0*.rawr\0All Files\0*.*\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&ofn)) {
                auto& ctx = GlobalContext::Get();
                std::wstring token = ctx.GetCurrentUserToken();
                if(ctx.GetRBAC().CheckPermission(token, L"hotpatch:apply")) {
                    // Load patch and apply
                    HANDLE hFile = CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                    if(hFile != INVALID_HANDLE_VALUE) {
                        DWORD sz = GetFileSize(hFile, nullptr);
                        std::vector<uint8_t> buf(sz);
                        DWORD rd; ReadFile(hFile, buf.data(), sz, &rd, nullptr);
                        CloseHandle(hFile);
                        std::wstring patchId = std::wstring(path);
                        if(ctx.GetHotPatcher().ApplyPatch(patchId, buf)) {
                            SendMessageW(pThis->m_hotpatchList, LB_ADDSTRING, 0, (LPARAM)patchId.c_str());
                        }
                    }
                }
            }
        }
        if(LOWORD(wp) == 9003) {
            int sel = (int)SendMessageW(pThis->m_hotpatchList, LB_GETCURSEL, 0, 0);
            if(sel >= 0) {
                wchar_t buf[256];
                SendMessageW(pThis->m_hotpatchList, LB_GETTEXT, sel, (LPARAM)buf);
                GlobalContext::Get().GetHotPatcher().RevertPatch(buf);
                SendMessageW(pThis->m_hotpatchList, LB_DELETESTRING, sel, 0);
            }
        }
    }
    return DefSubclassProc(hWnd, msg, wp, lp);
}
