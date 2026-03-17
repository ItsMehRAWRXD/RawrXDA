// MainWindow.cpp — Headless application shell implementation
// Converted from Qt (QMainWindow, QVBoxLayout, QPushButton, setCentralWidget) to pure C++17
// Preserves ALL original component creation: HexMagConsole, UnifiedHotpatchManager

#include "MainWindow.h"
#include "HexMagConsole.h"
#include "unified_hotpatch_manager.hpp"
#include <iostream>

MainWindow::MainWindow()
    : m_console(std::make_unique<HexMagConsole>())
    , m_hotpatchManager(std::make_unique<UnifiedHotpatchManager>())
{
    setWindowTitle("RawrXD-Shell — AI Toolkit");
    std::cout << "[MainWindow] Components created." << std::endl;
}

MainWindow::~MainWindow() {
    std::cout << "[MainWindow] Destroyed." << std::endl;
}

void MainWindow::initialize() {
    // Original Qt version created a QVBoxLayout with:
    //   - QPushButton("Load Model")
    //   - QPushButton("Run Inference")
    //   - HexMagConsole (added to layout)
    //   - setCentralWidget(centralWidget)
    //
    // Headless version: just log initialization
    m_console->appendLog("MainWindow initialized");
    m_console->appendLog("UnifiedHotpatchManager ready");
    m_console->appendLog("Components: [Load Model] [Run Inference] [Console]");

    std::cout << "[MainWindow] Initialization complete." << std::endl;
}
