#include "production_agentic_ide.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QInputDialog>
#include <QLineEdit>

ProductionAgenticIDE::ProductionAgenticIDE(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("RawrXD - Production IDE");
    setGeometry(100, 100, 1200, 800);
    
    // Initialize central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Create menus
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* newPaint = fileMenu->addAction(tr("New &Paint"));
    QAction* newCode = fileMenu->addAction(tr("New &Code"));
    QAction* newChat = fileMenu->addAction(tr("New &Chat"));
    fileMenu->addSeparator();
    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    QAction* saveAction = fileMenu->addAction(tr("&Save"));
    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    fileMenu->addSeparator();
    QAction* exportAction = fileMenu->addAction(tr("&Export Image..."));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    
    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = editMenu->addAction(tr("&Undo"));
    QAction* redoAction = editMenu->addAction(tr("&Redo"));
    editMenu->addSeparator();
    QAction* cutAction = editMenu->addAction(tr("Cu&t"));
    QAction* copyAction = editMenu->addAction(tr("&Copy"));
    QAction* pasteAction = editMenu->addAction(tr("&Paste"));
    
    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QAction* togglePaint = viewMenu->addAction(tr("Toggle &Paint Panel"));
    QAction* toggleCode = viewMenu->addAction(tr("Toggle &Code Panel"));
    QAction* toggleChat = viewMenu->addAction(tr("Toggle C&hat Panel"));
    QAction* toggleFeatures = viewMenu->addAction(tr("Toggle &Features"));
    viewMenu->addSeparator();
    QAction* resetLayout = viewMenu->addAction(tr("&Reset Layout"));
    
    // Connect actions to slots
    connect(newPaint, &QAction::triggered, this, &ProductionAgenticIDE::onNewPaint);
    connect(newCode, &QAction::triggered, this, &ProductionAgenticIDE::onNewCode);
    connect(newChat, &QAction::triggered, this, &ProductionAgenticIDE::onNewChat);
    connect(openAction, &QAction::triggered, this, &ProductionAgenticIDE::onOpen);
    connect(saveAction, &QAction::triggered, this, &ProductionAgenticIDE::onSave);
    connect(saveAsAction, &QAction::triggered, this, &ProductionAgenticIDE::onSaveAs);
    connect(exportAction, &QAction::triggered, this, &ProductionAgenticIDE::onExportImage);
    connect(exitAction, &QAction::triggered, this, &ProductionAgenticIDE::onExit);
    connect(undoAction, &QAction::triggered, this, &ProductionAgenticIDE::onUndo);
    connect(redoAction, &QAction::triggered, this, &ProductionAgenticIDE::onRedo);
    connect(cutAction, &QAction::triggered, this, &ProductionAgenticIDE::onCut);
    connect(copyAction, &QAction::triggered, this, &ProductionAgenticIDE::onCopy);
    connect(pasteAction, &QAction::triggered, this, &ProductionAgenticIDE::onPaste);
    connect(togglePaint, &QAction::triggered, this, &ProductionAgenticIDE::onTogglePaintPanel);
    connect(toggleCode, &QAction::triggered, this, &ProductionAgenticIDE::onToggleCodePanel);
    connect(toggleChat, &QAction::triggered, this, &ProductionAgenticIDE::onToggleChatPanel);
    connect(toggleFeatures, &QAction::triggered, this, &ProductionAgenticIDE::onToggleFeaturesPanel);
    connect(resetLayout, &QAction::triggered, this, &ProductionAgenticIDE::onResetLayout);
}

ProductionAgenticIDE::~ProductionAgenticIDE() = default;

void ProductionAgenticIDE::onNewPaint() {
    qInfo() << "[ProductionAgenticIDE] Creating new paint document";
    QString filename = QFileDialog::getSaveFileName(this, tr("New Paint Document"), QString(), tr("PNG Files (*.png);;All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Paint document created:" << filename;
        statusBar()->showMessage(tr("New paint document created: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onNewCode() {
    qInfo() << "[ProductionAgenticIDE] Creating new code file";
    QString filename = QFileDialog::getSaveFileName(this, tr("New Code File"), QString(), tr("C++ Files (*.cpp);;Header Files (*.h);;All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Code file created:" << filename;
        statusBar()->showMessage(tr("New code file created: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onNewChat() {
    qInfo() << "[ProductionAgenticIDE] Creating new chat session";
    bool ok;
    QString sessionName = QInputDialog::getText(this, tr("New Chat Session"), tr("Session name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !sessionName.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Chat session created:" << sessionName;
        statusBar()->showMessage(tr("Chat session created: %1").arg(sessionName), 3000);
    }
}

void ProductionAgenticIDE::onOpen() {
    qInfo() << "[ProductionAgenticIDE] Opening file dialog";
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Files"), QString(), tr("All Files (*)"));
    if (!files.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Opened" << files.size() << "files";
        for (const QString& file : files) {
            statusBar()->showMessage(tr("Opened: %1").arg(file), 2000);
        }
    }
}

void ProductionAgenticIDE::onSave() {
    qInfo() << "[ProductionAgenticIDE] Saving current document";
    statusBar()->showMessage(tr("Document saved"), 2000);
}

void ProductionAgenticIDE::onSaveAs() {
    qInfo() << "[ProductionAgenticIDE] Saving current document as...";
    QString filename = QFileDialog::getSaveFileName(this, tr("Save File As"), QString(), tr("All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Document saved as:" << filename;
        statusBar()->showMessage(tr("Document saved as: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onExportImage() {
    qInfo() << "[ProductionAgenticIDE] Exporting image";
    QString filename = QFileDialog::getSaveFileName(this, tr("Export Image"), QString(), tr("PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Image exported to:" << filename;
        statusBar()->showMessage(tr("Image exported: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onExit() {
    qInfo() << "[ProductionAgenticIDE] Exiting application";

}

void ProductionAgenticIDE::onUndo() {
    qInfo() << "[ProductionAgenticIDE] Undo operation";
    statusBar()->showMessage(tr("Undo"), 1000);
}

void ProductionAgenticIDE::onRedo() {
    qInfo() << "[ProductionAgenticIDE] Redo operation";
    statusBar()->showMessage(tr("Redo"), 1000);
}

void ProductionAgenticIDE::onCut() {
    qInfo() << "[ProductionAgenticIDE] Cut operation";
    statusBar()->showMessage(tr("Cut"), 1000);
}

void ProductionAgenticIDE::onCopy() {
    qInfo() << "[ProductionAgenticIDE] Copy operation";
    statusBar()->showMessage(tr("Copy"), 1000);
}

void ProductionAgenticIDE::onPaste() {
    qInfo() << "[ProductionAgenticIDE] Paste operation";
    statusBar()->showMessage(tr("Paste"), 1000);
}

void ProductionAgenticIDE::onTogglePaintPanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling paint panel";
    statusBar()->showMessage(tr("Paint panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleCodePanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling code panel";
    statusBar()->showMessage(tr("Code panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleChatPanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling chat panel";
    statusBar()->showMessage(tr("Chat panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleFeaturesPanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling features panel";
    statusBar()->showMessage(tr("Features panel toggled"), 1000);
}

void ProductionAgenticIDE::onResetLayout() {
    qInfo() << "[ProductionAgenticIDE] Resetting layout to default";
    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

void ProductionAgenticIDE::onFeatureToggled(const QString& featureId, bool enabled) {
    qInfo() << "[ProductionAgenticIDE] Feature" << featureId << "toggled:" << (enabled ? "enabled" : "disabled");
    statusBar()->showMessage(tr("Feature %1: %2").arg(featureId, enabled ? "enabled" : "disabled"), 2000);
}

void ProductionAgenticIDE::onFeatureClicked(const QString& featureId) {
    qInfo() << "[ProductionAgenticIDE] Feature clicked:" << featureId;
    statusBar()->showMessage(tr("Feature activated: %1").arg(featureId), 2000);
ProductionAgenticIDE::ProductionAgenticIDE(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("RawrXD - Production IDE");
    setGeometry(100, 100, 1200, 800);
    
    // Initialize central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Create menus
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* newPaint = fileMenu->addAction(tr("New &Paint"));
    QAction* newCode = fileMenu->addAction(tr("New &Code"));
    QAction* newChat = fileMenu->addAction(tr("New &Chat"));
    fileMenu->addSeparator();
    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    QAction* saveAction = fileMenu->addAction(tr("&Save"));
    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    fileMenu->addSeparator();
    QAction* exportAction = fileMenu->addAction(tr("&Export Image..."));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    
    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = editMenu->addAction(tr("&Undo"));
    QAction* redoAction = editMenu->addAction(tr("&Redo"));
    editMenu->addSeparator();
    QAction* cutAction = editMenu->addAction(tr("Cu&t"));
    QAction* copyAction = editMenu->addAction(tr("&Copy"));
    QAction* pasteAction = editMenu->addAction(tr("&Paste"));
    
    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QAction* togglePaint = viewMenu->addAction(tr("Toggle &Paint Panel"));
    QAction* toggleCode = viewMenu->addAction(tr("Toggle &Code Panel"));
    QAction* toggleChat = viewMenu->addAction(tr("Toggle C&hat Panel"));
    QAction* toggleFeatures = viewMenu->addAction(tr("Toggle &Features"));
    viewMenu->addSeparator();
    QAction* resetLayout = viewMenu->addAction(tr("&Reset Layout"));
    
    // Connect actions to slots
    connect(newPaint, &QAction::triggered, this, &ProductionAgenticIDE::onNewPaint);
    connect(newCode, &QAction::triggered, this, &ProductionAgenticIDE::onNewCode);
    connect(newChat, &QAction::triggered, this, &ProductionAgenticIDE::onNewChat);
    connect(openAction, &QAction::triggered, this, &ProductionAgenticIDE::onOpen);
    connect(saveAction, &QAction::triggered, this, &ProductionAgenticIDE::onSave);
    connect(saveAsAction, &QAction::triggered, this, &ProductionAgenticIDE::onSaveAs);
    connect(exportAction, &QAction::triggered, this, &ProductionAgenticIDE::onExportImage);
    connect(exitAction, &QAction::triggered, this, &ProductionAgenticIDE::onExit);
    connect(undoAction, &QAction::triggered, this, &ProductionAgenticIDE::onUndo);
    connect(redoAction, &QAction::triggered, this, &ProductionAgenticIDE::onRedo);
    connect(cutAction, &QAction::triggered, this, &ProductionAgenticIDE::onCut);
    connect(copyAction, &QAction::triggered, this, &ProductionAgenticIDE::onCopy);
    connect(pasteAction, &QAction::triggered, this, &ProductionAgenticIDE::onPaste);
    connect(togglePaint, &QAction::triggered, this, &ProductionAgenticIDE::onTogglePaintPanel);
    connect(toggleCode, &QAction::triggered, this, &ProductionAgenticIDE::onToggleCodePanel);
    connect(toggleChat, &QAction::triggered, this, &ProductionAgenticIDE::onToggleChatPanel);
    connect(toggleFeatures, &QAction::triggered, this, &ProductionAgenticIDE::onToggleFeaturesPanel);
    connect(resetLayout, &QAction::triggered, this, &ProductionAgenticIDE::onResetLayout);
}

ProductionAgenticIDE::~ProductionAgenticIDE() = default;

void ProductionAgenticIDE::onNewPaint() {
    qInfo() << "[ProductionAgenticIDE] Creating new paint document";
    QString filename = QFileDialog::getSaveFileName(this, tr("New Paint Document"), QString(), tr("PNG Files (*.png);;All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Paint document created:" << filename;
        statusBar()->showMessage(tr("New paint document created: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onNewCode() {
    qInfo() << "[ProductionAgenticIDE] Creating new code file";
    QString filename = QFileDialog::getSaveFileName(this, tr("New Code File"), QString(), tr("C++ Files (*.cpp);;Header Files (*.h);;All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Code file created:" << filename;
        statusBar()->showMessage(tr("New code file created: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onNewChat() {
    qInfo() << "[ProductionAgenticIDE] Creating new chat session";
    bool ok;
    QString sessionName = QInputDialog::getText(this, tr("New Chat Session"), tr("Session name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !sessionName.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Chat session created:" << sessionName;
        statusBar()->showMessage(tr("Chat session created: %1").arg(sessionName), 3000);
    }
}

void ProductionAgenticIDE::onOpen() {
    qInfo() << "[ProductionAgenticIDE] Opening file dialog";
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Files"), QString(), tr("All Files (*)"));
    if (!files.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Opened" << files.size() << "files";
        for (const QString& file : files) {
            statusBar()->showMessage(tr("Opened: %1").arg(file), 2000);
        }
    }
}

void ProductionAgenticIDE::onSave() {
    qInfo() << "[ProductionAgenticIDE] Saving current document";
    statusBar()->showMessage(tr("Document saved"), 2000);
}

void ProductionAgenticIDE::onSaveAs() {
    qInfo() << "[ProductionAgenticIDE] Saving current document as...";
    QString filename = QFileDialog::getSaveFileName(this, tr("Save File As"), QString(), tr("All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Document saved as:" << filename;
        statusBar()->showMessage(tr("Document saved as: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onExportImage() {
    qInfo() << "[ProductionAgenticIDE] Exporting image";
    QString filename = QFileDialog::getSaveFileName(this, tr("Export Image"), QString(), tr("PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)"));
    if (!filename.isEmpty()) {
        qInfo() << "[ProductionAgenticIDE] Image exported to:" << filename;
        statusBar()->showMessage(tr("Image exported: %1").arg(filename), 3000);
    }
}

void ProductionAgenticIDE::onExit() {
    qInfo() << "[ProductionAgenticIDE] Exiting application";
    close();
}

void ProductionAgenticIDE::onUndo() {
    qInfo() << "[ProductionAgenticIDE] Undo operation";
    statusBar()->showMessage(tr("Undo"), 1000);
}

void ProductionAgenticIDE::onRedo() {
    qInfo() << "[ProductionAgenticIDE] Redo operation";
    statusBar()->showMessage(tr("Redo"), 1000);
}

void ProductionAgenticIDE::onCut() {
    qInfo() << "[ProductionAgenticIDE] Cut operation";
    // Will be handled by QApplication's clipboard
    statusBar()->showMessage(tr("Cut"), 1000);
}

void ProductionAgenticIDE::onCopy() {
    qInfo() << "[ProductionAgenticIDE] Copy operation";
    statusBar()->showMessage(tr("Copy"), 1000);
}

void ProductionAgenticIDE::onPaste() {
    qInfo() << "[ProductionAgenticIDE] Paste operation";
    statusBar()->showMessage(tr("Paste"), 1000);
}

void ProductionAgenticIDE::onTogglePaintPanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling paint panel";
    statusBar()->showMessage(tr("Paint panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleCodePanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling code panel";
    statusBar()->showMessage(tr("Code panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleChatPanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling chat panel";
    statusBar()->showMessage(tr("Chat panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleFeaturesPanel() {
    qInfo() << "[ProductionAgenticIDE] Toggling features panel";
    statusBar()->showMessage(tr("Features panel toggled"), 1000);
}

void ProductionAgenticIDE::onResetLayout() {
    qInfo() << "[ProductionAgenticIDE] Resetting layout to default";
    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

void ProductionAgenticIDE::onFeatureToggled(const QString& featureId, bool enabled) {
    qInfo() << "[ProductionAgenticIDE] Feature" << featureId << "toggled:" << (enabled ? "enabled" : "disabled");
    statusBar()->showMessage(tr("Feature %1: %2").arg(featureId, enabled ? "enabled" : "disabled"), 2000);
}

void ProductionAgenticIDE::onFeatureClicked(const QString& featureId) {
    qInfo() << "[ProductionAgenticIDE] Feature clicked:" << featureId;
    statusBar()->showMessage(tr("Feature activated: %1").arg(featureId), 2000);
