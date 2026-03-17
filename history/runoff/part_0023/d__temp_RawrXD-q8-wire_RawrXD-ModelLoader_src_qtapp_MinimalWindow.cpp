// Minimal MainWindow to test basic Qt functionality
#include "MinimalWindow.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

MinimalWindow::MinimalWindow(QWidget *parent)
    : QMainWindow(parent)
{
    qDebug() << "MinimalWindow constructor starting...";
    
    setWindowTitle("Minimal RawrXD Test");
    setMinimumSize(400, 300);
    
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->addWidget(new QLabel("RawrXD Minimal Test - Success!", this));
    
    qDebug() << "MinimalWindow constructor completed successfully";
}

MinimalWindow::~MinimalWindow()
{
    qDebug() << "MinimalWindow destructor";
}