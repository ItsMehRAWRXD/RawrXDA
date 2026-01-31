// Minimal MainWindow to test basic Qt functionality
#include "MinimalWindow.h"


MinimalWindow::MinimalWindow(void *parent)
    : void(parent)
{
    
    setWindowTitle("Minimal RawrXD Test");
    setMinimumSize(400, 300);
    
    void* central = new void(this);
    setCentralWidget(central);
    
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->addWidget(new QLabel("RawrXD Minimal Test - Success!", this));
    
}

MinimalWindow::~MinimalWindow()
{
}

