// ============================================================================
// Win32IDE_SovereignCLI.h — Win32IDE integration for Sovereign CLI IDE
// ============================================================================

#pragma once

#include "SovereignCLIIDE.h"

class Win32IDE;

class Win32IDE_SovereignCLI
{
public:
    Win32IDE_SovereignCLI(Win32IDE* ide);
    ~Win32IDE_SovereignCLI();
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Tab management
    bool createCLITab();
    void destroyCLITab();
    void focusCLITab();
    
    // Command execution
    void executeCommand(const std::string& command);
    void executeCommandAsync(const std::string& command);
    
    // UI integration
    void updateUI();
    void resize(int width, int height);
    
    // Menu commands
    static void cmdShowCLI();
    static void cmdHideCLI();
    static void cmdExecuteCLI();
    
private:
    Win32IDE* m_ide;
    SovereignCLIIDE m_cliIDE;
    bool m_tabCreated;
    
    // UI handles
    HWND m_hwndTab;
    HWND m_hwndOutput;
    HWND m_hwndInput;
    
    // Callbacks
    void onCLIOutput(const std::string& output);
    void onCLIError(const std::string& error);
    void onCLICompleted(const SovereignCLIIDE::CommandResult& result);
};

// Global instance
extern Win32IDE_SovereignCLI* g_win32ideSovereignCLI;
