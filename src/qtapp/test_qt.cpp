#include <QApplication>
#include <QLabel>
#include <QDebug>

int main(int argc, char *argv[]) {
    qDebug() << "Test app starting...";
    QApplication app(argc, argv);
    QLabel label("Hello World");
    label.show();
    qDebug() << "Test app running...";
    return 0; // Don't block
}
