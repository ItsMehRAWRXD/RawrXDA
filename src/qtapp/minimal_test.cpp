#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>

int main(int argc, char* argv[])
{
    qDebug() << "Starting minimal Qt test...";
    
    QApplication app(argc, argv);
    
    QMainWindow window;
    window.setWindowTitle("Minimal Qt Test");
    window.resize(400, 300);
    
    QWidget* central = new QWidget(&window);
    QVBoxLayout* layout = new QVBoxLayout(central);
    
    QLabel* label = new QLabel("Qt is working!", central);
    layout->addWidget(label);
    
    window.setCentralWidget(central);
    window.show();
    
    qDebug() << "Qt test window shown successfully";
    
    return app.exec();
}