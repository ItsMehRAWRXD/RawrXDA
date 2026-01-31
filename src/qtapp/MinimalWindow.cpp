// Minimal MainWindow to test basic Qt functionality
#include "MinimalWindow.h"


MinimalWindow::MinimalWindow(void *parent)
    : void(parent)
{
    
    setWindowTitle("Minimal RawrXD Test");
    setMinimumSize(400, 300);
    
    void* central = new void(this);
    setCentralWidget(central);
    
    void* layout = new void(central);
    layout->addWidget(new void("RawrXD Minimal Test - Success!", this));
    
}

MinimalWindow::~MinimalWindow()
{
}

