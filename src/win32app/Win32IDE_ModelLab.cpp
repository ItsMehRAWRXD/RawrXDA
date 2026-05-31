#include "Win32IDE.h"

#include "../gguf_factory/gguf_writer_minimal.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <cwchar>
#include <string>

namespace
{

// Dialog + control IDs (local to this TU + .rc template).
static constexpr int IDD_MODEL_LAB = 6102;
static constexpr int IDC_ML_ARCH = 6103;
static constexpr int IDC_ML_DEEPTHINKING = 6104;
static constexpr int IDC_ML_DEEPRESEARCH = 6105;
static constexpr int IDC_ML_MAXMODE = 6106;
static constexpr int IDC_ML_OUTPATH = 6107;
static constexpr int IDC_ML_BROWSE = 6108;
static constexpr int IDC_ML_GENERATE = 6109;

static std::string wideToUtf8_(const wchar_t* ws)
{
    if (!ws)
        return {};
    const int lenW = (int)wcslen(ws);
    if (lenW <= 0)
        return {};
    int bytes = ::WideCharToMultiByte(CP_UTF8, 0, ws, lenW, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
        return {};
    std::string out((size_t)bytes, '\0');
    // std::string::data() is const char* pre-C++17; use &out[0] for a mutable buffer.
    ::WideCharToMultiByte(CP_UTF8, 0, ws, lenW, &out[0], bytes, nullptr, nullptr);
    return out;
}

static void setTextUtf8ToWideControl_(HWND hDlg, int controlId, const std::string& utf8)
{
    if (!hDlg)
        return;
    int wchars = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wchars <= 0)
        return;
    std::wstring w((size_t)wchars, L'\0');
    // std::wstring::data() is const wchar_t* pre-C++17; use &w[0] for a mutable buffer.
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], wchars);
    SetDlgItemTextW(hDlg, controlId, w.c_str());
}

static bool browseSavePath_(HWND hDlg, std::string& outUtf8Path)
{
    wchar_t fileBuf[MAX_PATH]{};
    wcscpy_s(fileBuf, L"rawrxd_profile.gguf");

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = L"GGUF Files (*.gguf)\0*.gguf\0All Files\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"gguf";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return false;

    outUtf8Path = wideToUtf8_(fileBuf);
    SetDlgItemTextW(hDlg, IDC_ML_OUTPATH, fileBuf);
    return !outUtf8Path.empty();
}

static INT_PTR CALLBACK modelLabDlgProc_(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
    (void)ide;

    switch (msg)
    {
        case WM_INITDIALOG:
        {
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);

            HWND hArch = GetDlgItem(hDlg, IDC_ML_ARCH);
            if (hArch)
            {
                SendMessageW(hArch, CB_ADDSTRING, 0, (LPARAM)L"llama");
                SendMessageW(hArch, CB_ADDSTRING, 0, (LPARAM)L"gptj");
                SendMessageW(hArch, CB_ADDSTRING, 0, (LPARAM)L"mpt");
                SendMessageW(hArch, CB_SETCURSEL, 0, 0);
            }
            CheckDlgButton(hDlg, IDC_ML_DEEPTHINKING, BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_ML_DEEPRESEARCH, BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_ML_MAXMODE, BST_UNCHECKED);
            return TRUE;
        }
        case WM_COMMAND:
        {
            const int id = LOWORD(wParam);
            if (id == IDC_ML_BROWSE)
            {
                std::string tmp;
                browseSavePath_(hDlg, tmp);
                return TRUE;
            }
            if (id == IDC_ML_GENERATE)
            {
                wchar_t pathW[MAX_PATH]{};
                GetDlgItemTextW(hDlg, IDC_ML_OUTPATH, pathW, MAX_PATH);
                std::string outPath = wideToUtf8_(pathW);
                if (outPath.empty())
                {
                    if (!browseSavePath_(hDlg, outPath))
                        return TRUE;
                }

                // Gather safe toggles.
                const bool deepThinking = (IsDlgButtonChecked(hDlg, IDC_ML_DEEPTHINKING) == BST_CHECKED);
                const bool deepResearch = (IsDlgButtonChecked(hDlg, IDC_ML_DEEPRESEARCH) == BST_CHECKED);
                const bool maxMode = (IsDlgButtonChecked(hDlg, IDC_ML_MAXMODE) == BST_CHECKED);

                // Architecture.
                std::string arch = "llama";
                HWND hArch = GetDlgItem(hDlg, IDC_ML_ARCH);
                if (hArch)
                {
                    const int sel = (int)SendMessageW(hArch, CB_GETCURSEL, 0, 0);
                    if (sel == 1)
                        arch = "gptj";
                    else if (sel == 2)
                        arch = "mpt";
                }

                RawrXD::GGUFFactory::GGUFWriterMinimal writer;
                writer.addString("general.architecture", arch);

                if (deepThinking)
                    writer.addBool("sovereign.deepthinking", true);
                if (deepResearch)
                    writer.addBool("sovereign.deepresearch", true);
                if (maxMode)
                    writer.addBool("sovereign.max_mode", true);

                if (!writer.writeMetadataOnly(outPath))
                {
                    const std::string err = "Failed to write GGUF: " + writer.lastError();
                    MessageBoxA(hDlg, err.c_str(), "Model Lab", MB_OK | MB_ICONERROR);
                    return TRUE;
                }

                setTextUtf8ToWideControl_(hDlg, IDC_ML_OUTPATH, outPath);
                MessageBoxW(hDlg, L"GGUF profile written successfully.", L"Model Lab", MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }

            if (id == IDOK || id == IDCANCEL)
            {
                EndDialog(hDlg, id);
                return TRUE;
            }
        }
        break;
    }

    return FALSE;
}

}  // namespace

void Win32IDE::showModelLabDialog()
{
    DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_MODEL_LAB), m_hwndMain, modelLabDlgProc_, (LPARAM)this);
}
