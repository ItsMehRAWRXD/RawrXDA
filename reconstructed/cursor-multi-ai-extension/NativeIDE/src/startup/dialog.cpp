#include "startup_dialog.h"
#include "ide_application.h"

NativeIDE::StartupDialog::StartupDialog(IDEApplication* app) 
    : m_app(app), m_hDlg(nullptr) {
}

NativeIDE::StartupDialog::~StartupDialog() = default;

INT_PTR NativeIDE::StartupDialog::Show(HWND hParent) {
    return DialogBoxParamW(
        m_app->GetInstance(),
        MAKEINTRESOURCEW(IDD_STARTUP),
        hParent,
        DialogProc,
        reinterpret_cast<LPARAM>(this)
    );
}

INT_PTR CALLBACK NativeIDE::StartupDialog::DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    StartupDialog* pThis = nullptr;
    
    if (uMsg == WM_INITDIALOG) {
        pThis = reinterpret_cast<StartupDialog*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
        pThis->m_hDlg = hDlg;
    } else {
        pThis = reinterpret_cast<StartupDialog*>(GetWindowLongPtrW(hDlg, DWLP_USER));
    }
    
    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    
    return FALSE;
}

INT_PTR StartupDialog::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (uMsg) {
        case WM_INITDIALOG:
            OnInitDialog();
            return TRUE;
            
        case WM_COMMAND:
            OnCommand(wParam);
            return TRUE;
            
        case WM_CLOSE:
            EndDialog(m_hDlg, IDCANCEL);
            return TRUE;
            
        default:
            return FALSE;
    }
}

void StartupDialog::OnInitDialog() {
    // Center the dialog
    RECT rcParent, rcDialog;
    GetWindowRect(GetParent(m_hDlg), &rcParent);
    GetWindowRect(m_hDlg, &rcDialog);
    
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDialog.right - rcDialog.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDialog.bottom - rcDialog.top)) / 2;
    
    SetWindowPos(m_hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    // Set dialog title
    SetWindowTextW(m_hDlg, L"Native IDE - Get Started");
    
    // Setup UI elements
    SetupRecentProjects();
    SetupProjectTemplates();
    UpdateUI();
}

void StartupDialog::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case IDC_OPEN_PROJECT:
            OnOpenProject();
            break;
            
        case IDC_CLONE_REPO:
            OnCloneRepository();
            break;
            
        case IDC_OPEN_FOLDER:
            OnOpenFolder();
            break;
            
        case IDC_NEW_PROJECT:
            OnCreateNewProject();
            break;
            
        case IDC_CONTINUE_EMPTY:
            OnContinueEmpty();
            break;
            
        case IDOK:
            if (ExecuteSelectedAction()) {
                EndDialog(m_hDlg, IDOK);
            }
            break;
            
        case IDCANCEL:
            EndDialog(m_hDlg, IDCANCEL);
            break;
    }
}

void StartupDialog::OnOpenProject() {
    m_selectedAction = StartupAction::OpenProject;
    
    OPENFILENAMEW ofn = {};
    wchar_t fileName[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hDlg;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Project Files\\0*.vcxproj;*.sln;*.cbp;Makefile\\0"
                      L"Visual Studio Projects\\0*.vcxproj;*.sln\\0"
                      L"Code::Blocks Projects\\0*.cbp\\0"
                      L"Makefiles\\0Makefile;makefile;*.mk\\0"
                      L"All Files\\0*.*\\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Open Project";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameW(&ofn)) {
        m_selectedPath = fileName;
        UpdateUI();
    }
}

void StartupDialog::OnCloneRepository() {
    m_selectedAction = StartupAction::CloneRepository;
    
    // Simple input dialog for repository URL
    // TODO: Create a proper clone dialog with URL input and local path selection
    wchar_t url[512] = {};
    if (DialogBoxParamW(m_app->GetInstance(), L"Clone Repository", m_hDlg, 
                       [](HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) -> INT_PTR {
                           // Simple input dialog implementation
                           UNREFERENCED_PARAMETER(lParam);
                           switch (msg) {
                               case WM_INITDIALOG:
                                   SetWindowTextW(hDlg, L"Clone Git Repository");
                                   return TRUE;
                               case WM_COMMAND:
                                   if (LOWORD(wParam) == IDOK) {
                                       EndDialog(hDlg, IDOK);
                                   } else if (LOWORD(wParam) == IDCANCEL) {
                                       EndDialog(hDlg, IDCANCEL);
                                   }
                                   return TRUE;
                           }
                           return FALSE;
                       }, 0) == IDOK) {
        
        m_repositoryUrl = IDEUtils::WStringToString(url);
        
        // Select local destination folder
        BROWSEINFOW bi = {};
        bi.hwndOwner = m_hDlg;
        bi.lpszTitle = L"Select destination folder for cloned repository";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        
        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
        if (pidl) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                m_selectedPath = path;
                UpdateUI();
            }
            CoTaskMemFree(pidl);
        }
    }
}

void StartupDialog::OnOpenFolder() {
    m_selectedAction = StartupAction::OpenFolder;
    
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hDlg;
    bi.lpszTitle = L"Select folder to open as project";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            m_selectedPath = path;
            UpdateUI();
        }
        CoTaskMemFree(pidl);
    }
}

void StartupDialog::OnCreateNewProject() {
    m_selectedAction = StartupAction::CreateNewProject;
    
    // TODO: Show new project dialog with template selection
    // For now, use a simple folder dialog and default template
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hDlg;
    bi.lpszTitle = L"Select location for new project";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            m_selectedPath = path;
            m_projectTemplate = "console-app";  // Default template
            UpdateUI();
        }
        CoTaskMemFree(pidl);
    }
}

void StartupDialog::OnContinueEmpty() {
    m_selectedAction = StartupAction::ContinueEmpty;
    UpdateUI();
}

bool StartupDialog::ExecuteSelectedAction() {
    switch (m_selectedAction) {
        case StartupAction::OpenProject:
            return m_app->OpenProject(m_selectedPath);
            
        case StartupAction::CloneRepository:
            return m_app->CloneRepository(m_repositoryUrl, m_selectedPath);
            
        case StartupAction::OpenFolder:
            return m_app->OpenFolder(m_selectedPath);
            
        case StartupAction::CreateNewProject: {
            std::wstring projectName = L"NewProject";  // TODO: Get from user input
            return m_app->CreateNewProject(projectName, m_selectedPath, m_projectTemplate);
        }
        
        case StartupAction::ContinueEmpty:
            return true;  // Just continue with empty workspace
            
        default:
            return false;
    }
}

void StartupDialog::SetupRecentProjects() {
    // TODO: Load and display recent projects
}

void StartupDialog::SetupProjectTemplates() {
    // TODO: Load available project templates
}

void StartupDialog::UpdateUI() {
    // TODO: Update UI based on selected action
    EnableWindow(GetDlgItem(m_hDlg, IDOK), m_selectedAction != StartupAction::None);
}