#pragma once
#ifndef RAWRXD_RESOURCE_H
#define RAWRXD_RESOURCE_H

// ============================================================================
// resource.h — Unified Menu/Command Resource IDs for RawrXD Win32 IDE
// ============================================================================
// All ID_* constants live here. Include this header instead of defining
// constexpr / #define duplicates in individual .cpp files.
// Ranges:
//   1001–1099  File menu
//   2001–2099  Edit menu
//   3001–3099  View menu
//   4001–4099  File browser / misc panels
//   5001–5099  Quick-open
//   6001–6099  Dialogs
//   7001–7099  Build menu
//   8001–8099  Tools menu
//   9001–9099  Help menu
// ============================================================================

// --- File menu (1001–1099) ---------------------------------------------------
#define ID_FILE_NEW             1001
#define ID_FILE_OPEN            1002
#define ID_FILE_SAVE            1003
#define ID_FILE_SAVEAS          1004
#define ID_FILE_SAVE_AS         1004   // alias (used in LinkFixes)
#define ID_FILE_EXIT            1005
#define ID_FILE_CLOSE           1006
#define ID_FILE_BROWSER_LIST    1010

// --- Edit menu (2001–2099) ---------------------------------------------------
#define ID_EDIT_UNDO            2001
#define ID_EDIT_REDO            2002
#define ID_EDIT_CUT             2003
#define ID_EDIT_COPY            2004
#define ID_EDIT_PASTE           2005
#define ID_EDIT_SELECT_ALL      2006
#define ID_EDIT_FIND            2007
#define ID_EDIT_REPLACE         2008

// --- View menu (3001–3099) ---------------------------------------------------
#define ID_VIEW_EXPLORER        3001
#define ID_VIEW_SEARCH          3002
#define ID_VIEW_TERMINAL        3003
#define ID_VIEW_OUTPUT          3004
#define ID_VIEW_PROBLEMS        3005
#define ID_VIEW_SIDEBAR         3010
#define ID_VIEW_TOOLBAR         3011
#define ID_VIEW_STATUS_BAR      3012
#define ID_VIEW_ZOOM_IN         3020
#define ID_VIEW_ZOOM_OUT        3021
#define ID_VIEW_ZOOM_RESET      3022

// --- Quick-open / dialogs (5001–6099) ----------------------------------------
#define ID_QUICKOPEN_SEARCH     5001
#define ID_QUICKOPEN_RESULTS    5002
#define IDD_QUICKOPEN           6001

// --- Build menu (7001–7099) --------------------------------------------------
#define ID_BUILD_COMPILE        7001
#define ID_BUILD_BUILD          7002
#define ID_BUILD_REBUILD        7003
#define ID_BUILD_CLEAN          7004
#define ID_BUILD_RUN            7005
#define ID_BUILD_DEBUG          7006

// --- Tools menu (8001–8099) --------------------------------------------------
#define ID_TOOLS_OPTIONS        8001
#define ID_TOOLS_PLUGINS        8002
#define ID_TOOLS_EXTENSIONS     8003
#define ID_TOOLS_SETTINGS       8004

// --- Help menu (9001–9099) ---------------------------------------------------
#define ID_HELP_CONTENTS        9001
#define ID_HELP_INDEX           9002
#define ID_HELP_SEARCH          9003
#define ID_HELP_ABOUT           9004

// --- VSCode extension (handled via vscode_extension_api.h, alias here) -------
// IDM_VSCEXT_API_STATUS et al. are defined in ../modules/vscode_extension_api.h

#endif // RAWRXD_RESOURCE_H
