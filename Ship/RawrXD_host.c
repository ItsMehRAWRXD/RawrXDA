#include <windows.h>
#include <wrl.h>
#include "WebView2.h"
#include <string>
#include <vector>

using namespace Microsoft::WRL;

static ICoreWebView2* g_webview = nullptr;
static std::wstring g_exeDir;

static std::wstring RunCli(const std::wstring& text) {
    WCHAR tmpPath[MAX_PATH];
    WCHAR tmpFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    GetTempFileNameW(tmpPath, L"rx", 0, tmpFile);

    HANDLE hf = CreateFileW(tmpFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (hf != INVALID_HANDLE_VALUE) {
        int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
        std::string utf8(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), utf8.data(), len, nullptr, nullptr);
        DWORD written = 0;
        WriteFile(hf, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
        CloseHandle(hf);
    }

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE readH = nullptr, writeH = nullptr;
    CreatePipe(&readH, &writeH, &sa, 0);
    SetHandleInformation(readH, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writeH;
    si.hStdError = writeH;
    PROCESS_INFORMATION pi{};

    std::wstring cmd = L"\"" + g_exeDir + L"RawrXD.exe\" \"" + tmpFile + L"\"";
    CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, g_exeDir.c_str(), &si, &pi);
    CloseHandle(writeH);

    std::string buffer;
    char chunk[4096];
    DWORD read = 0;
    while (ReadFile(readH, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
        buffer.append(chunk, chunk + read);
    }
    CloseHandle(readH);

    if (pi.hProcess) { WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); }
    if (pi.hThread) { CloseHandle(pi.hThread); }
    DeleteFileW(tmpFile);

    int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer.c_str(), (int)buffer.size(), nullptr, 0);
    std::wstring out(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, buffer.c_str(), (int)buffer.size(), out.data(), wlen);
    return out;
}

int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int) {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(h, path, MAX_PATH);
    WCHAR* p = wcsrchr(path, L'\\');
    if (p) { *(p + 1) = 0; g_exeDir = path; wcscpy(p + 1, L"RawrXD_gui.html"); }
    else { g_exeDir = L""; }

    ICoreWebView2Environment* env = nullptr;
    CreateCoreWebView2Environment(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&](HRESULT, IUnknown* envU)->HRESULT {
                envU->QueryInterface(&env);
                HWND w = CreateWindowW(L"STATIC", L"RawrXD IDE", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200, 1200, 700, 0, 0, h, 0);
                ICoreWebView2Controller* ctrl = nullptr;
                env->CreateCoreWebView2Controller(w, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [&](HRESULT, IUnknown* ctrlU)->HRESULT {
                        ctrlU->QueryInterface(&ctrl);
                        ICoreWebView2* wv = nullptr;
                        ctrl->get_CoreWebView2(&wv);
                        g_webview = wv;

                        EventRegistrationToken token{};
                        wv->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                            [&](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args)->HRESULT {
                                LPWSTR msg = nullptr;
                                args->get_WebMessageAsString(&msg);
                                std::wstring m = msg ? msg : L"";
                                CoTaskMemFree(msg);
                                const std::wstring prefix = L"RAW|";
                                if (m.rfind(prefix, 0) == 0) {
                                    std::wstring result = RunCli(m.substr(prefix.size()));
                                    if (g_webview) g_webview->PostWebMessageAsString(result.c_str());
                                }
                                return S_OK;
                            }).Get(), &token);

                        wv->Navigate(path);
                        return S_OK;
                    }).Get());
                return S_OK;
            }).Get());

    MSG m;
    while (GetMessage(&m, 0, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    return 0;
}
