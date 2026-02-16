// RawrXD_SettingsDialog.hpp - API Key & Extension Settings for Win32 IDE
// Pure Win32 - No Dependencies

#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include "RawrXD_APIKeyManager.hpp"

namespace RawrXD {

#define IDD_SETTINGS_DIALOG      5000
#define IDC_EDIT_API_KEY         5001
#define IDC_LABEL_API_STATUS     5002
#define IDC_CHECK_AMAZONQ        5003
#define IDC_CHECK_COPILOT        5004
#define IDC_BUTTON_TEST_KEY      5005
#define IDC_BUTTON_SAVE          5006
#define IDC_BUTTON_CANCEL        5007

// Dialog procedure for settings
INT_PTR CALLBACK SettingsDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static char apiKeyBuffer[512] = {};

    switch (uMsg) {
    case WM_INITDIALOG: {
        // Load current settings
        auto& mgr = APIKeyManager::Get();
        std::string key = mgr.getApiKey();
        strcpy_s(apiKeyBuffer, sizeof(apiKeyBuffer), key.c_str());
        SetDlgItemTextA(hwnd, IDC_EDIT_API_KEY, apiKeyBuffer);
        
        // Set checkboxes
        CheckDlgButton(hwnd, IDC_CHECK_AMAZONQ, mgr.isAmazonQEnabled() ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_COPILOT, mgr.isCopilotEnabled() ? BST_CHECKED : BST_UNCHECKED);
        
        // Show status
        std::string status = mgr.hasValidApiKey() ? "Valid API Key" : "No valid API key";
        SetDlgItemTextA(hwnd, IDC_LABEL_API_STATUS, status.c_str());
        
        return TRUE;
    }
    
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        
        if (id == IDC_BUTTON_TEST_KEY) {
            GetDlgItemTextA(hwnd, IDC_EDIT_API_KEY, apiKeyBuffer, sizeof(apiKeyBuffer));
            
            bool valid = strlen(apiKeyBuffer) > 0 && strstr(apiKeyBuffer, "key_") == apiKeyBuffer;
            std::string status = valid ? "✓ Valid API Key" : "✗ Invalid API Key";
            SetDlgItemTextA(hwnd, IDC_LABEL_API_STATUS, status.c_str());
            
            MessageBoxA(hwnd, 
                valid ? "API key format is valid!" : "API key format is invalid.",
                "API Key Test",
                valid ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
            
            return TRUE;
        }
        
        if (id == IDC_BUTTON_SAVE || id == IDOK) {
            // Save settings
            GetDlgItemTextA(hwnd, IDC_EDIT_API_KEY, apiKeyBuffer, sizeof(apiKeyBuffer));
            
            auto& mgr = APIKeyManager::Get();
            mgr.setApiKey(apiKeyBuffer);
            
            bool amazonQEnabled = IsDlgButtonChecked(hwnd, IDC_CHECK_AMAZONQ) == BST_CHECKED;
            bool copilotEnabled = IsDlgButtonChecked(hwnd, IDC_CHECK_COPILOT) == BST_CHECKED;
            
            mgr.enableAmazonQ(amazonQEnabled);
            mgr.enableGitHubCopilot(copilotEnabled);
            
            MessageBoxA(hwnd, 
                "Settings saved successfully!\n\nExtensions will be loaded on next startup.",
                "Success",
                MB_OK | MB_ICONINFORMATION);
            
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        
        if (id == IDC_BUTTON_CANCEL || id == IDCANCEL) {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        
        break;
    }
    
    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }
    
    return FALSE;
}

// Show settings dialog
bool ShowSettingsDialog(HWND hwndParent) {
    // Create dialog template in memory
    struct {
        DLGTEMPLATE template_;
        WORD menu;
        WORD class_;
        WORD title;
        WORD pointsize;
        WCHAR font[14];
    } dlg = {};
    
    dlg.template_.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.template_.cdit = 7;  // Number of controls
    dlg.template_.x = 0;
    dlg.template_.y = 0;
    dlg.template_.cx = 320;
    dlg.template_.cy = 200;
    
    // Use CreateDialogIndirect for dynamic dialog creation
    // For production use, you'd normally define this in a .rc file
    // This is a simplified version for demonstration
    
    // Alternative: Use task dialog for quick implementation
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = hwndParent;
    config.dwFlags = TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = L"RawrXD Settings";
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszMainInstruction = L"API Key & Extensions";
    
    wchar_t contentBuf[2048] = {};
    auto& mgr = APIKeyManager::Get();
    
    swprintf_s(contentBuf, L"Current API Key: %S\n\n"
                           L"Amazon Q: %s\n"
                           L"GitHub Copilot: %s\n\n"
                           L"To modify settings, edit:\n"
                           L"%%APPDATA%%\\RawrXD\\api_config.txt",
               mgr.getMaskedKey().c_str(),
               mgr.isAmazonQEnabled() ? L"Enabled" : L"Disabled",
               mgr.isCopilotEnabled() ? L"Enabled" : L"Disabled");
    
    config.pszContent = contentBuf;
    
    const TASKDIALOG_BUTTON buttons[] = {
        { 100, L"Edit Configuration File" },
        { IDOK, L"Close" }
    };
    
    config.pButtons = buttons;
    config.cButtons = 2;
    config.nDefaultButton = IDOK;
    
    int button = 0;
    TaskDialogIndirect(&config, &button, NULL, NULL);
    
    if (button == 100) {
        // Open config file in notepad
        char appData[MAX_PATH];
        if (GetEnvironmentVariableA("APPDATA", appData, MAX_PATH) > 0) {
            std::string configPath = std::string(appData) + "\\RawrXD\\api_config.txt";
            
            // Ensure file exists
            mgr.saveToConfig();
            
            std::string cmd = "notepad.exe \"" + configPath + "\"";
            system(cmd.c_str());
        }
    }
    
    return true;
}

} // namespace RawrXD
