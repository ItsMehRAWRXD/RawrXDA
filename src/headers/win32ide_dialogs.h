#ifndef WIN32IDE_DIALOGS_H_
#define WIN32IDE_DIALOGS_H_

#pragma once

#include <windows.h>
#include <string>
#include <cstdint>

// Forward declarations
struct HWND__;
typedef HWND__* HWND;

// ============================================================================
// CATEGORY 2 - Dialog/UI System Classes
// ============================================================================

/**
 * @namespace RawrXD::UI
 * @brief User interface components for RawrXD IDE
 */
namespace RawrXD::UI {

/**
 * @class MonacoSettingsDialog
 * @brief Settings dialog using Monaco editor paradigm
 */
class MonacoSettingsDialog {
public:
    /**
     * Constructor
     * @param hwnd Parent window handle (HWND*)
     */
    explicit MonacoSettingsDialog(HWND* hwnd);

    /**
     * Destructor
     */
    ~MonacoSettingsDialog();

    /**
     * Show settings dialog modally
     * @return Dialog result code as int64_t
     */
    int64_t showModal();

private:
    void* m_hwnd;
};

} // namespace RawrXD::UI

/**
 * @namespace rawrxd::thermal
 * @brief Thermal monitoring and dashboarding
 */
namespace rawrxd::thermal {

/**
 * @class ThermalDashboard
 * @brief Dashboard for thermal system monitoring
 */
class ThermalDashboard {
public:
    /**
     * Constructor
     * @param hwnd Parent window handle (HWND*)
     */
    explicit ThermalDashboard(HWND* hwnd);

    /**
     * Destructor
     */
    ~ThermalDashboard();

    /**
     * Show the thermal dashboard
     */
    void show();

private:
    void* m_hwnd;
};

} // namespace rawrxd::thermal

// ============================================================================
// Win32IDE Helper Methods and UI Support
// ============================================================================

/**
 * @brief Win32IDE dialog utility functions and helpers
 */
namespace Win32IDEDialogs {

/**
 * Create and display a message box dialog
 * @param hwnd Parent window handle
 * @param title Dialog title
 * @param message Message text
 * @param flags Dialog flags (MB_OK, MB_YESNO, etc.)
 * @return Dialog result
 */
int MessageBoxDialog(HWND hwnd, const std::string& title, const std::string& message, uint32_t flags);

/**
 * Create and display a file open dialog
 * @param hwnd Parent window handle
 * @param initialPath Initial directory path
 * @param filter File filter pattern
 * @return Selected file path, empty string if cancelled
 */
std::string FileOpenDialog(HWND hwnd, const std::string& initialPath, const std::string& filter);

/**
 * Create and display a file save dialog
 * @param hwnd Parent window handle
 * @param initialPath Initial directory path
 * @param filter File filter pattern
 * @param defaultExt Default file extension
 * @return Selected file path, empty string if cancelled
 */
std::string FileSaveDialog(HWND hwnd, const std::string& initialPath, const std::string& filter, const std::string& defaultExt);

/**
 * Create and display a folder picker dialog
 * @param hwnd Parent window handle
 * @param initialPath Initial directory path
 * @return Selected folder path, empty string if cancelled
 */
std::string FolderPickerDialog(HWND hwnd, const std::string& initialPath);

} // namespace Win32IDEDialogs

#endif // WIN32IDE_DIALOGS_H_
