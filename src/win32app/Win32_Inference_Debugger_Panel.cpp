// Win32_Inference_Debugger_Panel.cpp — Latent probe UI (top-k logits, Sovereign red)

// ListView_*W / WC_LISTVIEWW require Unicode and a full Windows include before commctrl.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00
#define _WIN32_IE 0x0500
#include "../RawrXD_Inference_Wrapper.h"
#include "../hotpatch/Inference_Debugger_Bridge.hpp"

#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <cwchar>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

// Define W-suffix constants if not present (project builds without UNICODE)
#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#endif
#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW  (LVM_FIRST + 77)
#endif
#ifndef LVM_INSERTCOLUMNW
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#endif

namespace
{
constexpr int IDC_INFDBG_LIST = 10001;
constexpr UINT_PTR TIMER_INFDBG = 1;
const wchar_t kPanelClass[] = L"RawrXD_Inference_Debugger_Panel";

HWND g_hwndPanel = nullptr;
HWND g_hwndList = nullptr;
bool g_classRegistered = false;

LRESULT CALLBACK InferenceDebuggerPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            INITCOMMONCONTROLSEX icc{};
            icc.dwSize = sizeof(icc);
            icc.dwICC = ICC_LISTVIEW_CLASSES;
            InitCommonControlsEx(&icc);

            g_hwndList = CreateWindowExW(
                0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_ALIGNLEFT, 10, 10, 560,
                300, hwnd, (HMENU)(UINT_PTR)IDC_INFDBG_LIST, GetModuleHandleW(nullptr), nullptr);
            if (!g_hwndList)
                return -1;
            SendMessageW(g_hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            auto addCol = [](HWND lv, int idx, const wchar_t* name, int w)
            {
                LVCOLUMNW col{};
                col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                col.fmt = LVCFMT_LEFT;
                col.cx = w;
                col.pszText = (LPWSTR)name;
                SendMessageW(lv, LVM_INSERTCOLUMNW, idx, (LPARAM)&col);
            };
            addCol(g_hwndList, 0, L"Rank", 50);
            addCol(g_hwndList, 1, L"Token ID", 90);
            addCol(g_hwndList, 2, L"Logit", 140);
            addCol(g_hwndList, 3, L"Sovereign", 100);
            addCol(g_hwndList, 4, L"Chosen", 70);

            SetTimer(hwnd, TIMER_INFDBG, 160, nullptr);
            return 0;
        }
        case WM_TIMER:
            if (wParam == TIMER_INFDBG && g_hwndList)
            {
                rawrxd::inference_debug::DebugFrame f{};
                if (!rawrxd::inference_debug::copyLatestFrame(f))
                    break;

                SendMessageW(g_hwndList, LVM_DELETEALLITEMS, 0, 0);
                wchar_t cell[128];
                for (int r = 0; r < rawrxd::inference_debug::kTopK; ++r)
                {
                    LVITEMW it{};
                    it.mask = LVIF_TEXT | LVIF_PARAM;
                    it.iItem = r;
                    it.lParam = static_cast<LPARAM>(
                        static_cast<LONG_PTR>(f.topId[r]));  // read back as lItemlParam in NM_CUSTOMDRAW
                    (void)std::swprintf(cell, 128, L"%d", r + 1);
                    it.pszText = cell;
                    SendMessageW(g_hwndList, LVM_INSERTITEMW, 0, (LPARAM)&it);

                    (void)std::swprintf(cell, 128, L"%d", f.topId[r]);
                    {
                        LV_ITEMW item{};
                        item.iSubItem = 1;
                        item.pszText = cell;
                        SendMessageW(g_hwndList, LVM_SETITEMTEXTW, r, (LPARAM)&item);
                    }

                    (void)std::swprintf(cell, 128, L"%.6g", static_cast<double>(f.topLogit[r]));
                    {
                        LV_ITEMW item{};
                        item.iSubItem = 2;
                        item.pszText = cell;
                        SendMessageW(g_hwndList, LVM_SETITEMTEXTW, r, (LPARAM)&item);
                    }

                    if (IsUnauthorized_NoDep(f.topId[r]))
                    {
                        wchar_t yes[] = L"suppress";
                        LV_ITEMW item{};
                        item.iSubItem = 3;
                        item.pszText = yes;
                        SendMessageW(g_hwndList, LVM_SETITEMTEXTW, r, (LPARAM)&item);
                    }
                    else
                    {
                        wchar_t no[] = L"";
                        LV_ITEMW item{};
                        item.iSubItem = 3;
                        item.pszText = no;
                        SendMessageW(g_hwndList, LVM_SETITEMTEXTW, r, (LPARAM)&item);
                    }

                    if (f.chosenId == f.topId[r])
                    {
                        wchar_t mark[] = L"*";
                        LV_ITEMW item{};
                        item.iSubItem = 4;
                        item.pszText = mark;
                        SendMessageW(g_hwndList, LVM_SETITEMTEXTW, r, (LPARAM)&item);
                    }
                }
            }
            break;
        case WM_NOTIFY:
        {
            NMHDR* nh = reinterpret_cast<NMHDR*>(lParam);
            if (nh->hwndFrom == g_hwndList && nh->code == NM_CUSTOMDRAW)
            {
                NMLVCUSTOMDRAW* lvcd = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
                if (lvcd->nmcd.dwDrawStage == CDDS_PREPAINT)
                    return CDRF_NOTIFYITEMDRAW;
                if (lvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                    return CDRF_NOTIFYSUBITEMDRAW;
                if (lvcd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
                {
                    const int row = static_cast<int>(lvcd->nmcd.dwItemSpec);
                    const int sub = lvcd->iSubItem;
                    const int tokId = static_cast<int>(static_cast<long long>(lvcd->nmcd.lItemlParam));
                    if (row >= 0 && row < rawrxd::inference_debug::kTopK && IsUnauthorized_NoDep(tokId))
                    {
                        if (sub == 1 || sub == 3)
                            lvcd->clrText = RGB(255, 0, 0);
                    }
                    return CDRF_DODEFAULT;
                }
                return CDRF_DODEFAULT;
            }
            break;
        }
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_INFDBG);
            g_hwndList = nullptr;
            g_hwndPanel = nullptr;
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool ensurePanelClass()
{
    if (g_classRegistered)
        return true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = InferenceDebuggerPanelProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kPanelClass;
    if (!RegisterClassExW(&wc))
        return false;
    g_classRegistered = true;
    return true;
}

}  // namespace

namespace rawrxd::inference_debug
{

void showDebuggerPanel(HWND ownerHwnd)
{
    (void)ownerHwnd;
    if (g_hwndPanel && IsWindow(g_hwndPanel))
    {
        SetForegroundWindow(g_hwndPanel);
        ShowWindow(g_hwndPanel, SW_SHOW);
        return;
    }
    if (!ensurePanelClass())
        return;

    g_hwndPanel = CreateWindowExW(
        WS_EX_APPWINDOW, kPanelClass, L"RawrXD — Inference Debugger (Sovereign 0x1751431337 / latent probe)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 600,
        400, ownerHwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (g_hwndPanel)
    {
        ShowWindow(g_hwndPanel, SW_SHOW);
        UpdateWindow(g_hwndPanel);
    }
}

void closeDebuggerPanel()
{
    if (g_hwndPanel && IsWindow(g_hwndPanel))
        DestroyWindow(g_hwndPanel);
}

}  // namespace rawrxd::inference_debug
