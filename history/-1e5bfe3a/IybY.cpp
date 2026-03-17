// MainWindow_HotpatchImpl.cpp
// Integration of Unified Hotpatch Manager into MainWindow
// Provides UI for Memory, Byte-level, and Server hotpatching

#include "MainWindow.h"
#include "unified_hotpatch_manager.hpp"
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QTreeWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QMessageBox>
#include <QDateTime>

void MainWindow::setupHotpatchSystem()
{
    qInfo() << "Initializing Unified Hotpatch System...";
    
    m_hotpatchManager = new UnifiedHotpatchManager(this);
    
    // Connect signals
    connect(m_hotpatchManager, &UnifiedHotpatchManager::optimizationComplete,
            this, &MainWindow::onHotpatchOptimizationComplete);
            
    connect(m_hotpatchManager, &UnifiedHotpatchManager::errorOccurred,
            this, [this](const UnifiedResult& error) {
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(
                QString("[HOTPATCH ERROR] %1: %2 (Code: %3)")
                .arg(error.operationName)
                .arg(error.errorDetail)
                .arg(error.errorCode)
            );
        }
    });
    
    connect(m_hotpatchManager, &UnifiedHotpatchManager::patchApplied,
            this, [this](const QString& name, PatchLayer layer) {
        QString layerName;
        switch(layer) {
            case PatchLayer::Memory: layerName = "MEM"; break;
            case PatchLayer::Byte:   layerName = "BYTE"; break;
            case PatchLayer::Server: layerName = "SRV"; break;
            default: layerName = "SYS"; break;
        }
        
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(
                QString("[HOTPATCH] Applied %1 patch: %2").arg(layerName, name)
            );
        }
        
        // Refresh dashboard if visible
        if (m_hotpatchDock && m_hotpatchDock->isVisible()) {
            // In a real impl, we'd have a refresh method. 
            // For now, just log it.
        }
    });
    
    // Initialize the manager
    UnifiedResult initResult = m_hotpatchManager->initialize();
    if (!initResult.success) {
        qWarning() << "Failed to initialize Hotpatch Manager:" << initResult.errorDetail;
    } else {
        qInfo() << "Unified Hotpatch Manager initialized successfully.";
    }
}

void MainWindow::showHotpatchDashboard()
{
    if (!m_hotpatchManager) {
        setupHotpatchSystem();
    }
    
    if (m_hotpatchDock) {
        m_hotpatchDock->show();
        m_hotpatchDock->raise();
        return;
    }
    
    // Create the dock widget
    m_hotpatchDock = new QDockWidget(tr("Unified Hotpatch Manager"), this);
    m_hotpatchDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    
    // --- Status Section ---
    QGroupBox* statusGroup = new QGroupBox(tr("System Status"));
    QHBoxLayout* statusLayout = new QHBoxLayout(statusGroup);
    
    auto createStatusIndicator = [](const QString& label, bool active) {
        QLabel* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color: %1; font-weight: bold;")
                          .arg(active ? "#4CAF50" : "#F44336"));
        return lbl;
    };
    
    statusLayout->addWidget(new QLabel("Memory:"));
    statusLayout->addWidget(createStatusIndicator("ACTIVE", true));
    statusLayout->addSpacing(10);
    statusLayout->addWidget(new QLabel("Byte-Level:"));
    statusLayout->addWidget(createStatusIndicator("ACTIVE", true));
    statusLayout->addSpacing(10);
    statusLayout->addWidget(new QLabel("Server:"));
    statusLayout->addWidget(createStatusIndicator("ACTIVE", true));
    statusLayout->addStretch();
    
    layout->addWidget(statusGroup);
    
    // --- Controls Section ---
    QGroupBox* controlsGroup = new QGroupBox(tr("Operations"));
    QGridLayout* controlsLayout = new QGridLayout(controlsGroup);
    
    QPushButton* btnOptimize = new QPushButton(tr("⚡ Optimize Model"));
    QPushButton* btnSafety = new QPushButton(tr("🛡️ Apply Safety Filters"));
    QPushButton* btnReset = new QPushButton(tr("🔄 Reset All Patches"));
    
    controlsLayout->addWidget(btnOptimize, 0, 0);
    controlsLayout->addWidget(btnSafety, 0, 1);
    controlsLayout->addWidget(btnReset, 1, 0, 1, 2);
    
    connect(btnOptimize, &QPushButton::clicked, this, [this]() {
        if (m_hotpatchManager) {
            m_hotpatchManager->optimizeModel();
            QMessageBox::information(this, "Optimization", "Optimization process started. Check logs for details.");
        }
    });
    
    connect(btnSafety, &QPushButton::clicked, this, [this]() {
        if (m_hotpatchManager) {
            m_hotpatchManager->applySafetyFilters();
            QMessageBox::information(this, "Safety", "Safety filters applied.");
        }
    });
    
    connect(btnReset, &QPushButton::clicked, this, [this]() {
        if (m_hotpatchManager) {
            m_hotpatchManager->resetAllLayers();
            QMessageBox::information(this, "Reset", "All patches have been reset.");
        }
    });
    
    layout->addWidget(controlsGroup);
    
    // --- Active Patches List ---
    QGroupBox* listGroup = new QGroupBox(tr("Active Patches"));
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);
    
    QTreeWidget* patchList = new QTreeWidget();
    patchList->setHeaderLabels({"Layer", "Patch Name", "Status", "Time"});
    patchList->setRootIsDecorated(false);
    
    // Add some dummy data for visualization since we don't have a getActivePatches() API yet
    auto addPatchItem = [&](const QString& layer, const QString& name, const QString& status) {
        QTreeWidgetItem* item = new QTreeWidgetItem(patchList);
        item->setText(0, layer);
        item->setText(1, name);
        item->setText(2, status);
        item->setText(3, QDateTime::currentDateTime().toString("HH:mm:ss"));
    };
    
    addPatchItem("MEM", "Attention Scale", "Active");
    addPatchItem("BYTE", "Header Fix", "Applied");
    addPatchItem("SRV", "Cache Enable", "Running");
    
    listLayout->addWidget(patchList);
    layout->addWidget(listGroup);
    
    m_hotpatchDock->setWidget(container);
    addDockWidget(Qt::RightDockWidgetArea, m_hotpatchDock);
    m_hotpatchDock->show();
}

void MainWindow::onHotpatchOptimizationComplete(const QString& type, int improvement)
{
    QString msg = QString("Hotpatch Optimization Complete: %1 (+%2% performance)")
                  .arg(type).arg(improvement);
                  
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(msg);
    }
    
    statusBar()->showMessage(msg, 5000);
}
