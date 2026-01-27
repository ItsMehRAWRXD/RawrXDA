// Stripped down MainWindow for startup testing
#include "MainWindowMinimal.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTextEdit>
#include <QSplitter>
#include <QTreeWidget>
#include <QDebug>

MainWindowMinimal::MainWindowMinimal(QWidget *parent)
    : QMainWindow(parent)
{
    qDebug() << "MainWindowMinimal constructor starting...";
    
    setWindowTitle("RawrXD IDE - Minimal Startup Test");
    resize(1200, 800);
    
    // Create basic menu
    QMenuBar* menu = menuBar();
    QMenu* fileMenu = menu->addMenu("&File");
    fileMenu->addAction("&New", this, &MainWindowMinimal::newFile);
    fileMenu->addAction("&Open", this, &MainWindowMinimal::openFile);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &MainWindowMinimal::close);
    
    // Create basic layout similar to VS Code
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    
    // Left sidebar (file explorer placeholder)
    QTreeWidget* explorer = new QTreeWidget(this);
    explorer->setHeaderLabel("Explorer");
    explorer->setMaximumWidth(200);
    explorer->setMinimumWidth(150);
    
    // Main editor area
    QTextEdit* editor = new QTextEdit(this);
    editor->setText("// RawrXD IDE - Minimal Startup Test\n// Success! Basic Qt layout working.\n// Ready for complex components...");
    
    // Right panel (placeholder)
    QTextEdit* output = new QTextEdit(this);
    output->setText("Output Panel\n\nIDE startup successful!");
    output->setMaximumWidth(250);
    
    // Add to splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(explorer);
    splitter->addWidget(editor);
    splitter->addWidget(output);
    splitter->setStretchFactor(1, 1); // Make editor area stretch
    
    mainLayout->addWidget(splitter);
    
    // Status bar
    statusBar()->showMessage("RawrXD IDE - Minimal startup complete", 5000);
    
    qDebug() << "MainWindowMinimal constructor completed successfully";
}

void MainWindowMinimal::newFile()
{
    qDebug() << "New file action";
}

void MainWindowMinimal::openFile()
{
    qDebug() << "Open file action";
}

MainWindowMinimal::~MainWindowMinimal()
{
    qDebug() << "MainWindowMinimal destructor";
}