#include "Win32IDE.h"

#include <commctrl.h>
#include <filesystem>

namespace fs = std::filesystem;

void Win32IDE::deleteItemInExplorer()
{
    const std::string path = getSelectedFilePath();
    if (!path.empty())
    {
        deleteItemInExplorer(path);
    }
}

void Win32IDE::renameItemInExplorer()
{
    const std::string path = getSelectedFilePath();
    if (!path.empty())
    {
        renameItemInExplorer(path);
    }
}

void Win32IDE::deleteItemInExplorer(const std::string& path)
{
    if (path.empty())
    {
        return;
    }

    const int confirm = MessageBoxA(m_hwndMain,
                                    ("Delete this item?\n\n" + path).c_str(),
                                    "Delete Explorer Item",
                                    MB_YESNO | MB_ICONWARNING);
    if (confirm != IDYES)
    {
        return;
    }

    std::error_code ec;
    const fs::path target(path);
    if (fs::is_directory(target, ec))
    {
        fs::remove_all(target, ec);
    }
    else
    {
        fs::remove(target, ec);
    }

    if (ec)
    {
        appendToOutput("[Explorer] Delete failed: " + ec.message() + "\n", "Explorer", OutputSeverity::Error);
        return;
    }

    refreshFileTree();
}

void Win32IDE::renameItemInExplorer(const std::string& path)
{
    if (path.empty())
    {
        return;
    }

    HWND tree = nullptr;
    if (m_hwndFileTree && IsWindow(m_hwndFileTree))
    {
        tree = m_hwndFileTree;
    }
    else if (m_hwndFileExplorer && IsWindow(m_hwndFileExplorer))
    {
        tree = m_hwndFileExplorer;
    }

    if (tree)
    {
        HTREEITEM selected = TreeView_GetSelection(tree);
        if (selected)
        {
            appendToOutput("[Explorer] Rename requested for: " + path + "\n", "Explorer", OutputSeverity::Info);
            SendMessage(tree, TVM_EDITLABEL, 0, reinterpret_cast<LPARAM>(selected));
            return;
        }
    }

    appendToOutput("[Explorer] Rename failed: no selected tree item\n", "Explorer", OutputSeverity::Warning);
}

void Win32IDE::discardChanges()
{
    if (!m_hwndGitStatus)
    {
        appendToOutput("[Git] No source control selection available for discard\n", "Git", OutputSeverity::Warning);
        return;
    }

    DWORD selStart = 0;
    DWORD selEnd = 0;
    SendMessageA(m_hwndGitStatus, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    const int lineIdx = (int)SendMessageA(m_hwndGitStatus, EM_LINEFROMCHAR, selStart, 0);

    char lineBuf[512] = {};
    *reinterpret_cast<WORD*>(lineBuf) = static_cast<WORD>(sizeof(lineBuf) - 2);
    const int lineLen = (int)SendMessageA(m_hwndGitStatus, EM_GETLINE, lineIdx, (LPARAM)lineBuf);
    if (lineLen <= 0)
    {
        appendToOutput("[Git] Select a changed file line before discarding\n", "Git", OutputSeverity::Warning);
        return;
    }

    lineBuf[lineLen] = '\0';
    std::string line(lineBuf);
    const size_t firstNonSpace = line.find_first_not_of(" \t");
    const size_t pathStart = firstNonSpace == std::string::npos ? std::string::npos
                                                                 : line.find_first_not_of(" \t", firstNonSpace + 1);
    if (pathStart == std::string::npos)
    {
        appendToOutput("[Git] Could not parse selected change\n", "Git", OutputSeverity::Warning);
        return;
    }

    std::string targetFile = line.substr(pathStart);
    while (!targetFile.empty() &&
           (targetFile.back() == '\r' || targetFile.back() == '\n' || targetFile.back() == ' '))
    {
        targetFile.pop_back();
    }

    if (targetFile.empty() || targetFile == "(none)" || targetFile == "(clean)" || targetFile.rfind("===", 0) == 0)
    {
        appendToOutput("[Git] Select a changed file line before discarding\n", "Git", OutputSeverity::Warning);
        return;
    }

    const int confirm = MessageBoxA(m_hwndMain,
                                    ("Discard changes for\n\n" + targetFile + "?").c_str(),
                                    "Discard Changes",
                                    MB_YESNO | MB_ICONWARNING);
    if (confirm != IDYES)
    {
        return;
    }

    std::string result;
    const bool ok = executeGitCommand(std::string("git restore -- \"") + targetFile + "\"", result);
    appendToOutput("[Git] Discard changes: " + (ok ? result : std::string("command failed")) + "\n",
                   "Git",
                   ok ? OutputSeverity::Info : OutputSeverity::Error);
    refreshSourceControlView();
    refreshFileTree();
}
