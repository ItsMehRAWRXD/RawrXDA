/**
 * @file Win32IDE_IELabels.h
 * @brief ESP IE labels — single source of truth for Win32 IDE control identification.
 *
 * Used for source digestion, reverse engineering, and automation: every
 * main menu, submenu, breadcrumb, file explorer, and key UI element is
 * identified by a stable string so tools can discover and drive the IDE.
 * All menus, breadcrumb bar, and file explorer are manifested and
 * connected end-to-end with these labels. Menu commands are dispatched
 * via WM_COMMAND -> onCommand -> routeCommandUnified (SSOT command_registry).
 */
#pragma once

#ifndef RAWRXD_IDE_IE_LABELS_H
#define RAWRXD_IDE_IE_LABELS_H

#define RAWRXD_IDE_LABEL_MAIN_WINDOW     "RawrXD.IDE.MainWindow"
#define RAWRXD_IDE_LABEL_MAIN_MENU       "RawrXD.IDE.MainMenu"
#define RAWRXD_IDE_LABEL_MENU_FILE       "RawrXD.IDE.Menu.File"
#define RAWRXD_IDE_LABEL_MENU_EDIT       "RawrXD.IDE.Menu.Edit"
#define RAWRXD_IDE_LABEL_MENU_VIEW       "RawrXD.IDE.Menu.View"
#define RAWRXD_IDE_LABEL_MENU_TERMINAL   "RawrXD.IDE.Menu.Terminal"
#define RAWRXD_IDE_LABEL_MENU_TOOLS      "RawrXD.IDE.Menu.Tools"
#define RAWRXD_IDE_LABEL_MENU_MODULES    "RawrXD.IDE.Menu.Modules"
#define RAWRXD_IDE_LABEL_MENU_HELP       "RawrXD.IDE.Menu.Help"
#define RAWRXD_IDE_LABEL_MENU_AUDIT      "RawrXD.IDE.Menu.Audit"
#define RAWRXD_IDE_LABEL_MENU_GIT        "RawrXD.IDE.Menu.Git"
#define RAWRXD_IDE_LABEL_MENU_AGENT     "RawrXD.IDE.Menu.Agent"

#define RAWRXD_IDE_LABEL_BREADCRUMB_BAR "RawrXD.IDE.BreadcrumbBar"
#define RAWRXD_IDE_LABEL_FILE_EXPLORER  "RawrXD.IDE.FileExplorer"
#define RAWRXD_IDE_LABEL_SIDEBAR        "RawrXD.IDE.Sidebar"
#define RAWRXD_IDE_LABEL_ACTIVITY_BAR   "RawrXD.IDE.ActivityBar"
#define RAWRXD_IDE_LABEL_TAB_BAR        "RawrXD.IDE.TabBar"
#define RAWRXD_IDE_LABEL_EDITOR         "RawrXD.IDE.Editor"
#define RAWRXD_IDE_LABEL_STATUS_BAR     "RawrXD.IDE.StatusBar"
#define RAWRXD_IDE_LABEL_TOOLBAR        "RawrXD.IDE.Toolbar"
#define RAWRXD_IDE_LABEL_OUTPUT_PANEL   "RawrXD.IDE.OutputPanel"

#endif /* RAWRXD_IDE_IE_LABELS_H */
