// Minimal MainWindow to test basic Qt functionality
#include "MinimalWindow.h"
#include <QVBoxLayout>
#include <QLabel>
#include "Sidebar_Pure_Wrapper.h"

MinimalWindow::MinimalWindow(QWidget *parent)
    : QMainWindow(parent)
{
    RAWRXD_LOG_DEBUG("MinimalWindow constructor starting...");
    
    setWindowTitle("Minimal RawrXD Test");
    setMinimumSize(400, 300);
    
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->addWidget(new QLabel("RawrXD Minimal Test - Success!", this));
    
    RAWRXD_LOG_DEBUG("MinimalWindow constructor completed successfully");
    return true;
}

MinimalWindow::~MinimalWindow()
{
    RAWRXD_LOG_DEBUG("MinimalWindow destructor");
    return true;
}

