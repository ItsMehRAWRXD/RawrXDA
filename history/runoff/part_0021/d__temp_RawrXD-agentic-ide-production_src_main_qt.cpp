#include <QApplication>
#include <QWidget>
#include "IDEIntegration.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    IDEIntegration* ide = IDEIntegration::getInstance();
    ide->initialize();

    QWidget window;
    window.setWindowTitle("RawrXD IDE (Minimal)");
    window.resize(800, 600);
    window.show();

    int rc = app.exec();
    ide->shutdown();
    return rc;
}
