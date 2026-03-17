#include "MainWindow_v5.h"
#include "ai_digestion_panel.hpp"
#include <QVBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QSettings>
#include <QIcon>

MainWindow_v5::MainWindow_v5(QWidget* parent)
    : QMainWindow(parent)
    , m_digestionPanel(nullptr)
{
    setWindowTitle("RawrXD Agentic IDE v5.0");
    setGeometry(100, 100, 1280, 800);
    setupUI();
    setupMenuBar();
    setupStatusBar();
    restoreWindowState();
}

void MainWindow_v5::setupUI()
{
    m_tabWidget = new QTabWidget(this);
    
    // Create digestion panel
    m_digestionPanel = new AIDigestionPanel(this);
    m_tabWidget->addTab(m_digestionPanel, "AI Digestion & Training");
    
    setCentralWidget(m_tabWidget);
}

void MainWindow_v5::setupMenuBar()
{
    auto fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("E&xit", this, &QWidget::close);
    
    auto helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        // Show about dialog
    });
}

void MainWindow_v5::setupStatusBar()
{
    statusBar()->showMessage("Ready - AI Digestion & Training System");
}

void MainWindow_v5::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow_v5::restoreWindowState()
{
    QSettings settings;
    restoreGeometry(settings.value("MainWindow/geometry", saveGeometry()).toByteArray());
    restoreState(settings.value("MainWindow/windowState", saveState()).toByteArray());
}

void MainWindow_v5::saveWindowState()
{
    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/windowState", saveState());
}




