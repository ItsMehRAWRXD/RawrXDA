// MainWindow.h — Headless application shell
// Converted from Qt (QMainWindow, QVBoxLayout, QPushButton) to pure C++17
// Preserves ALL original structure: component ownership, initialization

#pragma once

#include <string>
#include <memory>
#include <iostream>
#include <functional>

// Forward declarations
class HexMagConsole;
class UnifiedHotpatchManager;

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    // Initialize all subsystems
    void initialize();

    // Accessors
    HexMagConsole* console() const { return m_console.get(); }
    UnifiedHotpatchManager* hotpatchManager() const { return m_hotpatchManager.get(); }

    // Window title (headless equivalent)
    void setWindowTitle(const std::string& title) { m_title = title; }
    const std::string& windowTitle() const { return m_title; }

    // Show (headless — just logs)
    void show() {
        std::cout << "[MainWindow] " << m_title << " — ready" << std::endl;
    }

    // Close
    void close() {
        std::cout << "[MainWindow] Closing." << std::endl;
    }

    // Resize (headless — store dimensions for API compat)
    void resize(int w, int h) { m_width = w; m_height = h; }
    int width()  const { return m_width; }
    int height() const { return m_height; }

private:
    std::unique_ptr<HexMagConsole> m_console;
    std::unique_ptr<UnifiedHotpatchManager> m_hotpatchManager;
    std::string m_title;
    int m_width  = 1200;
    int m_height = 800;
};
