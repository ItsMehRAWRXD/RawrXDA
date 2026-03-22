// Win32IDE_HandleExtensionCommand.cpp — single TU for Win32IDE::handleExtensionCommand
// (avoids LNK2001 when ExtensionsPanel/Sidebar split; no *_fallback name so strict stub filters keep it)

#include "Win32IDE.h"

void Win32IDE::handleExtensionCommand(int commandId)
{
    constexpr int IDC_EXT_DETAILS = 6052;
    constexpr int IDC_EXT_INSTALL = 6053;
    constexpr int IDC_EXT_UNINSTALL = 6054;
    constexpr int IDC_EXT_INSTALL_VSIX = 6055;

    constexpr int IDM_EXT_INSTALL = 11810;
    constexpr int IDM_EXT_ENABLE = 11811;
    constexpr int IDM_EXT_DISABLE = 11812;
    constexpr int IDM_EXT_UNINSTALL = 11813;
    constexpr int IDM_EXT_RELOAD = 11814;

    auto getSelectedExtensionId = [this]() -> std::string
    {
        if (!m_hwndExtensionsList || !IsWindow(m_hwndExtensionsList))
        {
            return {};
        }

        const int selected = ListView_GetNextItem(m_hwndExtensionsList, -1, LVNI_SELECTED);
        if (selected < 0)
        {
            return {};
        }

        char nameBuf[256] = {};
        ListView_GetItemText(m_hwndExtensionsList, selected, 0, nameBuf, static_cast<int>(sizeof(nameBuf)));
        return std::string(nameBuf);
    };

    switch (commandId)
    {
        case IDC_EXT_INSTALL_VSIX:
            installFromVSIXFile();
            return;

        case IDC_EXT_INSTALL:
        case IDM_EXT_INSTALL:
        {
            const std::string selected = getSelectedExtensionId();
            if (!selected.empty())
            {
                installExtension(selected);
            }
            else
            {
                installFromVSIXFile();
            }
            return;
        }

        case IDC_EXT_UNINSTALL:
        case IDM_EXT_UNINSTALL:
        {
            const std::string selected = getSelectedExtensionId();
            if (!selected.empty())
            {
                uninstallExtension(selected);
            }
            return;
        }

        case IDC_EXT_DETAILS:
        {
            const std::string selected = getSelectedExtensionId();
            if (!selected.empty())
            {
                showExtensionDetails(selected);
            }
            return;
        }

        case IDM_EXT_ENABLE:
        {
            const std::string selected = getSelectedExtensionId();
            if (!selected.empty())
            {
                enableExtension(selected);
            }
            return;
        }

        case IDM_EXT_DISABLE:
        {
            const std::string selected = getSelectedExtensionId();
            if (!selected.empty())
            {
                disableExtension(selected);
            }
            return;
        }

        case IDM_EXT_RELOAD:
            loadInstalledExtensions();
            return;

        default:
            return;
    }
}
